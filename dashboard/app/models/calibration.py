import json
import math
import uuid

from dataclasses import dataclass
from app.extensions import db
from app.models.synchronizable import Synchronizable
from app.utils.expr import ExpressionParser


_std_env = dict(
    pi=math.pi,
    sin=math.sin,
    cos=math.cos,
    tan=math.tan,
    asin=math.asin,
    acos=math.acos,
    atan=math.atan,
    sqrt=math.sqrt,
    sample=0,
    MAX_STROKE=0,
    MAX_TRAVEL=0,
)


@dataclass
class CalibrationMethod(db.Model, Synchronizable):
    id: uuid.UUID = db.Column(db.Uuid(), primary_key=True, default=uuid.uuid4)
    name: str = db.Column(db.String, nullable=False)
    description: str = db.Column(db.String)
    properties_raw = db.Column('data', db.String, nullable=False)

    properties: dict

    @property
    def properties(self) -> dict:
        return json.loads(self.properties_raw)

    @properties.setter
    def properties(self, value: dict):
        self.properties_raw = json.dumps(value)

    def validate(self) -> bool:
        props = self.properties

        # Handle lookup type calibration
        if props.get('type') == 'as5600-lookup':
            return self._validate_lookup(props)

        # Handle expression type calibration (default)
        return self._validate_expression(props)

    def _validate_lookup(self, props: dict) -> bool:
        """Validate lookup table properties"""
        points = props.get('points', [])

        # Must have at least 2 points
        if not isinstance(points, list) or len(points) < 2:
            return False

        # Validate each point has deg and mm as numbers
        for point in points:
            if not isinstance(point, dict):
                return False
            if 'deg' not in point or 'mm' not in point:
                return False
            try:
                deg = float(point['deg'])
                mm = float(point['mm'])
                if deg < 0 or deg > 360:
                    return False
                if mm < 0:
                    return False
            except (TypeError, ValueError):
                return False

        return True

    def _validate_expression(self, props: dict) -> bool:
        """Validate expression-based properties"""
        env = dict(_std_env)
        for input in props.get('inputs', []):
            env[input] = 1
        parser = ExpressionParser(env)
        for k, v in props.get('intermediates', {}).items():
            if not parser.validate(v):
                return False
            env[k] = 1
        parser = ExpressionParser(env)
        return parser.validate(props.get('expression', ''))


@dataclass
class Calibration(db.Model, Synchronizable):
    id: uuid.UUID = db.Column(db.Uuid(), primary_key=True, default=uuid.uuid4)
    name: str = db.Column(db.String, nullable=False)
    method_id: uuid.UUID = db.Column(db.Uuid(),
                                     db.ForeignKey('calibration_method.id'),
                                     nullable=False)
    inputs_raw = db.Column('inputs', db.String, nullable=False)

    inputs: dict[str: float]

    @property
    def inputs(self):
        return json.loads(self.inputs_raw)

    @inputs.setter
    def inputs(self, value: dict[str: float]):
        self.inputs_raw = json.dumps(value)

    def validate(self) -> bool:
        cm = CalibrationMethod.get(self.method_id)
        if not cm:
            return False

        # Handle lookup type calibration
        if cm.properties.get('type') == 'as5600-lookup':
            return self._validate_lookup_inputs()

        # Handle expression type calibration (default)
        for k in cm.properties.get('inputs', []):
            if k not in self.inputs:
                return False
        return True

    def _validate_lookup_inputs(self) -> bool:
        """Validate lookup table points in inputs"""
        points = self.inputs.get('points', [])

        # Must have at least 2 points
        if not isinstance(points, list) or len(points) < 2:
            return False

        # Validate each point has deg and mm
        for point in points:
            if not isinstance(point, dict):
                return False
            if 'deg' not in point or 'mm' not in point:
                return False
            try:
                float(point['deg'])
                float(point['mm'])
            except (TypeError, ValueError):
                return False

        return True
