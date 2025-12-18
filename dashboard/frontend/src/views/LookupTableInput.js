var m = require("mithril")

var LookupTableInput = {
  validateDeg: function(value) {
    if (value === null || value === undefined || value === "") return "Required"
    var num = parseFloat(value)
    if (isNaN(num)) return "Must be a number"
    if (num < 0 || num > 360) return "Must be 0-360"
    return ""
  },

  validateMm: function(value) {
    if (value === null || value === undefined || value === "") return "Required"
    var num = parseFloat(value)
    if (isNaN(num)) return "Must be a number"
    if (num < 0) return "Must be >= 0"
    return ""
  },

  addPoint: function(points) {
    points.push({deg: null, mm: null})
    m.redraw()
  },

  removePoint: function(points, index) {
    if (points.length > 2) {
      points.splice(index, 1)
      m.redraw()
    }
  },

  parseCSV: function(text, points) {
    // Parse CSV/semicolon-separated format:
    // "0, 0; 17, 6.72; 49.9, 21.62" or newline-separated
    var newPoints = []

    // Split by semicolon or newline
    var lines = text.replace(/;/g, '\n').split('\n')

    for (var i = 0; i < lines.length; i++) {
      var line = lines[i].trim()
      if (!line) continue

      var parts = line.split(',').map(function(s) { return s.trim() })
      if (parts.length >= 2) {
        var deg = parseFloat(parts[0])
        var mm = parseFloat(parts[1])
        if (!isNaN(deg) && !isNaN(mm)) {
          newPoints.push({deg: deg, mm: mm})
        }
      }
    }

    if (newPoints.length >= 2) {
      // Clear existing and add new points
      points.length = 0
      newPoints.forEach(function(p) { points.push(p) })
      m.redraw()
      return true
    }
    return false
  },

  view: function(vnode) {
    var points = vnode.attrs.points
    var self = this

    return m(".lookup-table-input", [
      m(".lookup-table-header", [
        m("span", "Lookup Table"),
        m(".lookup-table-actions", [
          m("button.lookup-btn", {
            onclick: function() { self.addPoint(points) },
            type: "button"
          }, "+ Add"),
          m("label.lookup-btn.file-upload", [
            "Import CSV",
            m("input", {
              type: "file",
              accept: ".csv,.txt",
              onchange: function(e) {
                if (e.target.files[0]) {
                  e.target.files[0].text().then(function(text) {
                    self.parseCSV(text, points)
                  })
                }
              }
            })
          ])
        ])
      ]),
      m(".lookup-table-labels", [
        m("span.lookup-label", "Degrees"),
        m("span.lookup-label", "Stroke (mm)"),
        m("span.lookup-label-spacer")
      ]),
      m(".lookup-table-points",
        points.map(function(point, index) {
          var degError = self.validateDeg(point.deg)
          var mmError = self.validateMm(point.mm)
          return m(".lookup-table-row", {key: index}, [
            m("input.lookup-input", {
              type: "number",
              step: "any",
              placeholder: "deg",
              value: point.deg,
              class: degError ? "input-error" : "",
              oninput: function(e) { point.deg = parseFloat(e.target.value) }
            }),
            m("input.lookup-input", {
              type: "number",
              step: "any",
              placeholder: "mm",
              value: point.mm,
              class: mmError ? "input-error" : "",
              oninput: function(e) { point.mm = parseFloat(e.target.value) }
            }),
            m("button.lookup-remove", {
              onclick: function() { self.removePoint(points, index) },
              disabled: points.length <= 2,
              type: "button"
            }, "x")
          ])
        })
      ),
      m(".lookup-table-help", "Min 2 points. Format: degrees (0-360), stroke (mm)")
    ])
  }
}

module.exports = LookupTableInput
