import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
  id: root
  visible: true
  width: 1365
  height: 768
  title: "UAV Acoustic"
  color: "#123A6E" // deep blue background

  // ---- palette (approx. like reference GUI photo) ----
  QtObject {
    id: pal
    readonly property color bg: "#123A6E"
    readonly property color panel: "#1B4F8F"         // main panel fill
    readonly property color panelDark: "#0F2F5C"     // darker fill
    readonly property color border: "#9DB7D7"        // light border
    readonly property color borderDark: "#0B1F3D"    // dark border
    readonly property color text: "#EAF2FF"
    readonly property color textDim: "#BFD3EE"
    readonly property color header: "#2C77D1"        // accent header
    readonly property color yellow: "#FFE600"        // alert
    readonly property color plotBg: "#0B0F14"
  }

  // ---- layout constants ----
  property int outerPad: 18
  property int leftPanelW: 260
  property int rightPanelW: 320
  property int innerBorderW: 2

  // ---- detection values (channel 1 for now) ----
  // IMPORTANT: coerce telemetry values to Number. This prevents cases where one view updates
  // (string formatting) but the plot buffer rejects values as NaN or sees stale QVariant types.
  function numOrNaN(v) {
    var n = Number(v)
    return isNaN(n) ? NaN : n
  }

  // Single source of truth for channel-1 probability.
  property real detectProbCh1: (telemetry && telemetry.pDetectLatest !== undefined) ? numOrNaN(telemetry.pDetectLatest) : NaN
  property real detectThreshold: (telemetry && telemetry.pOn !== undefined) ? numOrNaN(telemetry.pOn) : 0.65
  property bool uavDetected: detectProbCh1 >= detectThreshold

  function fmtLabelValue(label, value) { return label + " " + value }

  // ---- outer frame ----
  Rectangle {
    anchors.fill: parent
    anchors.margins: root.outerPad
    color: "transparent"
    border.color: pal.border
    border.width: root.innerBorderW

    RowLayout {
      id: rootRow
      anchors.fill: parent
      spacing: 0

      // ---------------- Left panel (reserved, must exist) ----------------
      Rectangle {
        id: leftPanel
        Layout.preferredWidth: root.leftPanelW
        Layout.minimumWidth: root.leftPanelW
        Layout.maximumWidth: root.leftPanelW
        Layout.fillHeight: true
        color: pal.panel
        border.color: pal.border
        border.width: root.innerBorderW

        // header bar
        Rectangle {
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.top: parent.top
          height: 44
          color: pal.header
          border.color: pal.border
          border.width: 1

          Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            text: "КОМАНДНЫЙ ПУНКТ"
            font.pixelSize: 16
            font.bold: true
            color: pal.text
          }
        }

        // reserved body
        Rectangle {
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.top: parent.top
          anchors.topMargin: 44
          anchors.bottom: parent.bottom
          color: "transparent"

          Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 12
            text: "(левая панель — резерв)"
            font.pixelSize: 12
            color: pal.textDim
          }
        }
      }

      // ---------------- Center panel (banner + PCEN + plot) ----------------
      Rectangle {
        id: centerPanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: pal.panelDark
        border.color: pal.border
        border.width: root.innerBorderW

        ColumnLayout {
          anchors.fill: parent
          spacing: 0

          // Top zone
          Rectangle {
            id: bannerZone
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            Layout.minimumHeight: 70
            color: pal.panel
            border.color: pal.border
            border.width: root.innerBorderW

            Rectangle {
              id: banner
              visible: root.uavDetected
              width: Math.min(420, bannerZone.width * 0.6)
              height: 44
              radius: 2
              color: pal.yellow
              anchors.centerIn: parent

              Text {
                anchors.centerIn: parent
                text: "ОБНАРУЖЕН БПЛА"
                font.pixelSize: 16
                font.bold: true
                color: "black"
              }
            }
          }

          // Middle zone: PCEN
          Rectangle {
            id: pcenZone
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 260
            color: pal.panelDark
            border.color: pal.border
            border.width: root.innerBorderW

            Image {
              id: pcenImage
              anchors.fill: parent
              anchors.margins: 10
              fillMode: Image.Stretch
              cache: false
              source: "image://pcen/latest?f=" + ((telemetry && telemetry.frameId !== undefined) ? telemetry.frameId : 0)
            }

            Text {
              anchors.centerIn: parent
              visible: pcenImage.status !== Image.Ready
              text: "PCEN MEL"
              font.pixelSize: 18
              color: pal.textDim
            }
          }

          // Bottom zone: p_detect plot
          Rectangle {
            id: pdZone
            Layout.fillWidth: true
            Layout.preferredHeight: 190
            Layout.minimumHeight: 160
            color: pal.panelDark
            border.color: pal.border
            border.width: root.innerBorderW

            // current probability (always visible)
            Text {
              anchors.left: parent.left
              anchors.bottom: parent.bottom
              anchors.leftMargin: 16
              anchors.bottomMargin: 10
              text: "P = " + root.detectProbCh1.toFixed(3)
              font.pixelSize: 13
              color: pal.textDim
            }

            Canvas {
              id: plot
              anchors.fill: parent
              anchors.margins: 10

              // history buffer
              property int maxN: 120
              property var history: [] // [{p: number}]

              function pushP(p) {
                if (isNaN(p)) return
                history.push({ p: p })
                if (history.length > maxN) history.shift()
              }

              function yFromP(p) {
                // clamp [0..1]
                var pp = Math.max(0.0, Math.min(1.0, p))
                return height - 28 - pp * (height - 48)
              }

              onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                // background
                ctx.fillStyle = pal.plotBg
                ctx.fillRect(0, 0, width, height)

                // title
                ctx.fillStyle = "#CFE2FF"
                ctx.font = "14px sans-serif"
                ctx.fillText("p_detect", 8, 18)

                // thresholds
                var pOn = root.detectThreshold
                var pOff = (telemetry && telemetry.pOff !== undefined) ? telemetry.pOff : Math.max(0.0, pOn - 0.15)

                ctx.lineWidth = 1
                ctx.setLineDash([6, 4])

                ctx.strokeStyle = "#ffd24d"
                var yOn = yFromP(pOn)
                ctx.beginPath(); ctx.moveTo(0, yOn); ctx.lineTo(width, yOn); ctx.stroke()

                ctx.strokeStyle = "#4db2ff"
                var yOff = yFromP(pOff)
                ctx.beginPath(); ctx.moveTo(0, yOff); ctx.lineTo(width, yOff); ctx.stroke()

                ctx.setLineDash([])

                // curve (draw even for 1 point so the plot never looks empty)
                if (history.length >= 1) {
                  ctx.strokeStyle = "#00d28a"
                  ctx.lineWidth = 2

                  if (history.length === 1) {
                    // single point marker
                    var x0 = 0
                    var y0 = yFromP(history[0].p)
                    ctx.beginPath()
                    ctx.arc(x0, y0, 3, 0, Math.PI * 2)
                    ctx.fillStyle = "#00d28a"
                    ctx.fill()
                  } else {
                    ctx.beginPath()
                    for (var i = 0; i < history.length; ++i) {
                      var x = (i / (maxN - 1)) * width
                      var y = yFromP(history[i].p)
                      if (i === 0) ctx.moveTo(x, y)
                      else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                  }
                }

                // last
                var pFallback = (telemetry && telemetry.pDetectLatest !== undefined) ? root.numOrNaN(telemetry.pDetectLatest) : root.detectProbCh1
                var pLast = (history.length > 0) ? history[history.length - 1].p : pFallback
                if (!isNaN(pLast)) {
                  ctx.fillStyle = "#d0d0d0"
                  ctx.fillText("last = " + pLast.toFixed(3), 8, height - 8)
                }
              }

              Connections {
                target: telemetry
                function onUpdated() {
                  // Sample directly from telemetry to avoid any binding/timing edge cases.
                  var p = (telemetry && telemetry.pDetectLatest !== undefined) ? root.numOrNaN(telemetry.pDetectLatest) : NaN
                  plot.pushP(p)
                  plot.requestPaint()
                }
              }

              // Fallback: if the backend does not emit `updated()` reliably (or at all),
              // sample the bound value at a modest rate to keep the plot and labels live.
              Timer {
                interval: 100
                repeat: true
                running: true
                onTriggered: {
                  var p = (telemetry && telemetry.pDetectLatest !== undefined) ? root.numOrNaN(telemetry.pDetectLatest) : NaN
                  plot.pushP(p)
                  plot.requestPaint()
                }
              }

              Component.onCompleted: {
                // seed with current value to avoid empty plot
                var p = (telemetry && telemetry.pDetectLatest !== undefined) ? root.numOrNaN(telemetry.pDetectLatest) : NaN
                plot.pushP(p)
                plot.requestPaint()
              }
            }
          }
        }
      }

      // ---------------- Right panel (must contain channels 1..4) ----------------
      Rectangle {
        id: rightPanel
        Layout.preferredWidth: root.rightPanelW
        Layout.minimumWidth: root.rightPanelW
        Layout.maximumWidth: root.rightPanelW
        Layout.fillHeight: true
        color: pal.panel
        border.color: pal.border
        border.width: root.innerBorderW

        ColumnLayout {
          anchors.fill: parent
          spacing: 0

          // helper component: one channel block
          Component {
            id: channelBlock
            Rectangle {
              Layout.fillWidth: true
              Layout.fillHeight: true
              color: pal.panel
              border.color: pal.border
              border.width: root.innerBorderW
              clip: true

              property string title: ""
              property real pVal: NaN
              property string typeText: "-"
              property string azText: "-"

              Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                  text: title
                  font.pixelSize: 16
                  font.bold: true
                  color: pal.text
                }

                Text {
                  text: (isNaN(pVal) ? "P = -" : ("P = " + pVal.toFixed(2)))
                  font.pixelSize: 14
                  color: pal.text
                }
                Text { text: "Тип: " + typeText; font.pixelSize: 14; color: pal.textDim }
                Text { text: "Азимут: " + azText; font.pixelSize: 14; color: pal.textDim }
              }
            }
          }

          Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: channelBlock
            onLoaded: {
              item.title = "1 канал"
              item.pVal = Qt.binding(function(){ return root.detectProbCh1 })
              item.typeText = "Mavik_3T" // TODO: bind from telemetry
              item.azText = "150 градусов" // TODO: bind from telemetry
            }
          }

          Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: channelBlock
            onLoaded: {
              item.title = "2 канал"
              item.pVal = NaN
              item.typeText = "-"
              item.azText = "-"
            }
          }

          Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: channelBlock
            onLoaded: {
              item.title = "3 канал"
              item.pVal = NaN
              item.typeText = "-"
              item.azText = "-"
            }
          }

          Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: channelBlock
            onLoaded: {
              item.title = "4 канал"
              item.pVal = NaN
              item.typeText = "-"
              item.azText = "-"
            }
          }
        }
      }
    }
  }
}
