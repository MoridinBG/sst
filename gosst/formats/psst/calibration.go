package psst

import (
	"encoding/json"
	"fmt"
	"math"
	"sort"

	"github.com/antonmedv/expr"
	"github.com/antonmedv/expr/vm"
	"github.com/google/uuid"
)

// Evaluator interface for calibration evaluation strategies
type Evaluator interface {
	Evaluate(sample float64) (float64, error)
}

// LookupPoint represents a single point in a lookup table calibration
type LookupPoint struct {
	Deg float64 `codec:"," json:"deg"`
	Mm  float64 `codec:"," json:"mm"`
}

// AS5600 sensor outputs 12-bit values (0-4095) for 360 degrees
const AS5600ToDegrees = 360.0 / 4096.0

// calibrationMethodParams holds the method-specific configuration
type calibrationMethodParams struct {
	// Type determines the evaluation strategy: "expression" (default) or "as5600-lookup"
	Type string `codec:"," json:"type,omitempty"`

	// Expression-based calibration fields
	Inputs        []string          `codec:"," json:"inputs,omitempty"`
	Intermediates map[string]string `codec:"," json:"intermediates,omitempty"`
	Expression    string            `codec:"," json:"expression,omitempty"`

	// Lookup table calibration fields
	Points []LookupPoint `codec:"," json:"points,omitempty"`
}

// CalibrationMethod stores calibration method metadata and parameters
type CalibrationMethod struct {
	Id          uuid.UUID `codec:"-" db:"id"          json:"id"`
	Name        string    `codec:"," db:"name"        json:"name"        binding:"required"`
	Description string    `codec:"," db:"description" json:"description"`
	RawData     string    `codec:"-" db:"data"        json:"-"`
	calibrationMethodParams
}

// Calibration represents a calibration instance with a specific method and inputs
type Calibration struct {
	Id        uuid.UUID              `codec:"-" db:"id"        json:"id"`
	Name      string                 `codec:"," db:"name"      json:"name"      binding:"required"`
	MethodId  uuid.UUID              `codec:"," db:"method_id" json:"method_id" binding:"required"`
	RawInputs string                 `codec:"-" db:"inputs"    json:"-"`
	Inputs    map[string]interface{} `codec:","                json:"inputs"    binding:"required"`
	Method    *CalibrationMethod     `codec:"-"                json:"method,omitempty"`
	evaluator Evaluator
}

type calibrations struct {
	FrontCalibration *Calibration `json:"front"`
	RearCalibration  *Calibration `json:"rear"`
}

// ============================================================================
// Expression-based Evaluator
// ============================================================================

var stdenv = map[string]interface{}{
	"pi":     math.Pi,
	"sin":    math.Sin,
	"cos":    math.Cos,
	"tan":    math.Tan,
	"asin":   math.Asin,
	"acos":   math.Acos,
	"atan":   math.Atan,
	"sqrt":   math.Sqrt,
	"sample": 0,
}

// ExpressionEvaluator evaluates calibration using mathematical expressions
type ExpressionEvaluator struct {
	program *vm.Program
	env     map[string]interface{}
}

// NewExpressionEvaluator creates an expression evaluator from method and calibration inputs
func NewExpressionEvaluator(method *CalibrationMethod, inputs map[string]interface{}, maxStroke, maxTravel float64) (*ExpressionEvaluator, error) {
	env := make(map[string]interface{})

	// Copy standard environment
	for k, v := range stdenv {
		env[k] = v
	}

	// Set input values from calibration
	for k, v := range inputs {
		if fv, ok := v.(float64); ok {
			env[k] = fv
		}
	}
	env["MAX_STROKE"] = maxStroke
	env["MAX_TRAVEL"] = maxTravel

	// Calculate intermediates
	for k, v := range method.Intermediates {
		p, err := expr.Compile(v, expr.Env(env))
		if err != nil {
			return nil, fmt.Errorf("compiling intermediate %q: %w", k, err)
		}

		out, err := expr.Run(p, env)
		if err != nil {
			return nil, fmt.Errorf("evaluating intermediate %q: %w", k, err)
		}

		env[k] = out.(float64)
	}

	// Compile main expression
	program, err := expr.Compile(method.Expression, expr.Env(env))
	if err != nil {
		return nil, fmt.Errorf("compiling expression: %w", err)
	}

	return &ExpressionEvaluator{
		program: program,
		env:     env,
	}, nil
}

// Evaluate computes stroke from sensor sample using expression
func (e *ExpressionEvaluator) Evaluate(sample float64) (float64, error) {
	e.env["sample"] = sample
	out, err := expr.Run(e.program, e.env)
	if err != nil {
		return math.NaN(), err
	}

	return out.(float64), nil
}

// ============================================================================
// Lookup Table Evaluator
// ============================================================================

// LookupEvaluator evaluates calibration using linear interpolation on lookup points
type LookupEvaluator struct {
	points []LookupPoint // sorted by Deg
}

// NewLookupEvaluator creates a lookup evaluator from calibration inputs
func NewLookupEvaluator(method *CalibrationMethod, inputs map[string]interface{}) (*LookupEvaluator, error) {
	// Points can come from method (default template) or inputs (user-provided)
	var points []LookupPoint

	// Try to get points from inputs first (user-provided override)
	if pointsRaw, ok := inputs["points"]; ok {
		pointsList, ok := pointsRaw.([]interface{})
		if !ok {
			return nil, fmt.Errorf("inputs.points must be an array")
		}
		for i, p := range pointsList {
			pm, ok := p.(map[string]interface{})
			if !ok {
				return nil, fmt.Errorf("points[%d] must be an object", i)
			}
			deg, ok1 := pm["deg"].(float64)
			mm, ok2 := pm["mm"].(float64)
			if !ok1 || !ok2 {
				return nil, fmt.Errorf("points[%d] must have deg and mm as numbers", i)
			}
			points = append(points, LookupPoint{Deg: deg, Mm: mm})
		}
	} else if len(method.Points) > 0 {
		// Fall back to method's default points
		points = make([]LookupPoint, len(method.Points))
		copy(points, method.Points)
	}

	if len(points) < 2 {
		return nil, fmt.Errorf("lookup table requires at least 2 points, got %d", len(points))
	}

	// Sort points by degree
	sort.Slice(points, func(i, j int) bool {
		return points[i].Deg < points[j].Deg
	})

	return &LookupEvaluator{points: points}, nil
}

// Evaluate computes stroke from sensor sample using linear interpolation
func (e *LookupEvaluator) Evaluate(sample float64) (float64, error) {
	// Convert AS5600 raw value (0-4095) to degrees (0-360)
	deg := sample * AS5600ToDegrees

	return e.interpolate(deg), nil
}

// interpolate performs linear interpolation on sorted points
// If deg is outside the range, extrapolates linearly using the nearest segment
func (e *LookupEvaluator) interpolate(deg float64) float64 {
	n := len(e.points)
	if n == 0 {
		return 0
	}
	if n == 1 {
		return e.points[0].Mm
	}

	// Handle extrapolation below first point
	if deg <= e.points[0].Deg {
		p0, p1 := e.points[0], e.points[1]
		if p1.Deg == p0.Deg {
			return p0.Mm
		}
		t := (deg - p0.Deg) / (p1.Deg - p0.Deg)
		return p0.Mm + t*(p1.Mm-p0.Mm)
	}

	// Find the segment containing deg
	for i := 0; i < n-1; i++ {
		p0, p1 := e.points[i], e.points[i+1]
		if deg <= p1.Deg {
			if p1.Deg == p0.Deg {
				return p0.Mm
			}
			t := (deg - p0.Deg) / (p1.Deg - p0.Deg)
			return p0.Mm + t*(p1.Mm-p0.Mm)
		}
	}

	// Extrapolate beyond last point using last segment
	p0, p1 := e.points[n-2], e.points[n-1]
	if p1.Deg == p0.Deg {
		return p1.Mm
	}
	t := (deg - p0.Deg) / (p1.Deg - p0.Deg)
	return p0.Mm + t*(p1.Mm-p0.Mm)
}

// ============================================================================
// CalibrationMethod methods
// ============================================================================

func (m *CalibrationMethod) ProcessRawData() error {
	if err := json.Unmarshal([]byte(m.RawData), &m.calibrationMethodParams); err != nil {
		return err
	}
	return nil
}

func (m *CalibrationMethod) DumpRawData() error {
	rd, err := json.Marshal(m.calibrationMethodParams)
	if err != nil {
		return err
	}
	m.RawData = string(rd)
	return nil
}

// IsLookup returns true if this method uses lookup table evaluation
func (m *CalibrationMethod) IsLookup() bool {
	return m.Type == "as5600-lookup"
}

// ============================================================================
// Calibration methods
// ============================================================================

func (c *Calibration) ProcessRawInputs() error {
	if err := json.Unmarshal([]byte(c.RawInputs), &c.Inputs); err != nil {
		return err
	}
	return nil
}

func (c *Calibration) DumpRawInput() error {
	rd, err := json.Marshal(c.Inputs)
	if err != nil {
		return err
	}
	c.RawInputs = string(rd)
	return nil
}

// Prepare creates the appropriate evaluator based on the calibration method type
func (c *Calibration) Prepare(maxStroke, maxTravel float64) error {
	var err error

	if c.Method.IsLookup() {
		c.evaluator, err = NewLookupEvaluator(c.Method, c.Inputs)
	} else {
		c.evaluator, err = NewExpressionEvaluator(c.Method, c.Inputs, maxStroke, maxTravel)
	}

	return err
}

// Evaluate computes the stroke value from the raw sensor sample
func (c *Calibration) Evaluate(sample float64) (float64, error) {
	if c.evaluator == nil {
		return math.NaN(), fmt.Errorf("calibration not prepared")
	}
	return c.evaluator.Evaluate(sample)
}

// ============================================================================
// Loader
// ============================================================================

func LoadCalibrations(data []byte, linkage Linkage) (*Calibration, *Calibration, error) {
	var cs calibrations
	if err := json.Unmarshal(data, &cs); err != nil {
		return nil, nil, err
	}
	if cs.FrontCalibration != nil {
		if err := cs.FrontCalibration.Prepare(linkage.MaxFrontStroke, linkage.MaxFrontTravel); err != nil {
			return nil, nil, err
		}
	}
	if cs.RearCalibration != nil {
		if err := cs.RearCalibration.Prepare(linkage.MaxRearStroke, linkage.MaxRearTravel); err != nil {
			return nil, nil, err
		}
	}

	return cs.FrontCalibration, cs.RearCalibration, nil
}
