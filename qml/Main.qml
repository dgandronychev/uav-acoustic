import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
  visible: true
  width: 1200
  height: 720
  title: "UAV Acoustic (Step 0)"

  Column {
    anchors.fill: parent
    spacing: 10
    padding: 10

    Text {
      text: "p_detect = " + telemetry.pDetectLatest.toFixed(3)
            + " | FSM = " + telemetry.fsmState
      font.pixelSize: 18
    }

    Image {
      width: parent.width
      height: 320
      fillMode: Image.Stretch
      cache: false
      source: "image://pcen/latest?f=" + telemetry.frameId
    }

    Rectangle {
      width: parent.width
      height: 220
      radius: 8
      border.width: 1

      Canvas {
        id: plot
        anchors.fill: parent

        onPaint: {
          var ctx = getContext("2d");
          ctx.reset();

          // background
          ctx.fillStyle = "#101010";
          ctx.fillRect(0, 0, width, height);

          // axes bottom
          ctx.strokeStyle = "#404040";
          ctx.beginPath();
          ctx.moveTo(0, height - 1);
          ctx.lineTo(width, height - 1);
          ctx.stroke();

          var data = telemetry.timeline;
          if (!data || data.length < 2) return;

          // helper: map p in [0..1] to y
          function yFromP(p) {
            if (p < 0) p = 0;
            if (p > 1) p = 1;
            return (1.0 - p) * (height - 10) + 5;
          }

          // ---- plot p_detect polyline ----
          ctx.strokeStyle = "#00ff7f";
          ctx.lineWidth = 2;
          ctx.beginPath();
          for (var i = 0; i < data.length; ++i) {
            var p = data[i]["p"];
            var x = i * (width / (data.length - 1));
            var y = yFromP(p);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
          }
          ctx.stroke();

          // ---- thresholds (p_on / p_off) ----
          var pOn  = (telemetry.pOn  !== undefined) ? telemetry.pOn  : 0.65;
          var pOff = (telemetry.pOff !== undefined) ? telemetry.pOff : 0.45;

          ctx.lineWidth = 1;
          ctx.setLineDash([6, 4]);

          // p_on
          ctx.strokeStyle = "#ffd24d";
          var yOn = yFromP(pOn);
          ctx.beginPath();
          ctx.moveTo(0, yOn);
          ctx.lineTo(width, yOn);
          ctx.stroke();

          // p_off
          ctx.strokeStyle = "#4db2ff";
          var yOff = yFromP(pOff);
          ctx.beginPath();
          ctx.moveTo(0, yOff);
          ctx.lineTo(width, yOff);
          ctx.stroke();

          ctx.setLineDash([]);

          // ---- START/END markers (minimal: draw at "now" on right edge) ----
          // eventStarted/eventEnded are "one-tick" flags: true only on the tick when event happened.
          var started = (telemetry.eventStarted !== undefined) ? telemetry.eventStarted : false;
          var ended   = (telemetry.eventEnded   !== undefined) ? telemetry.eventEnded   : false;

          function drawMarker(label, color, yText) {
            var x = width - 2;
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();

            ctx.fillStyle = color;
            ctx.font = "bold 12px sans-serif";
            ctx.fillText(label, Math.max(2, x - 45), yText);
          }

          if (started) drawMarker("START", "#ff6b6b", 14);
          if (ended)   drawMarker("END",   "#c084fc", 28);
        }

        Connections {
          target: telemetry
          function onUpdated() { plot.requestPaint(); }
        }
      }
    }

    Text {
      text: "UI: heatmap + p_detect + thresholds + START/END markers"
      color: "#808080"
    }
  }
}
