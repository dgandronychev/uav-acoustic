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
      text: "p_detect = " + telemetry.pDetectLatest.toFixed(3) + " | FSM = " + telemetry.fsmState
      font.pixelSize: 18
    }

    Image {
      width: parent.width
      height: 320
      fillMode: Image.Stretch
      cache: false
      source: "image://pcen/latest"
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

          // axes
          ctx.strokeStyle = "#404040";
          ctx.beginPath();
          ctx.moveTo(0, height-1);
          ctx.lineTo(width, height-1);
          ctx.stroke();

          var data = telemetry.timeline;
          if (!data || data.length < 2) return;

          // polyline
          ctx.strokeStyle = "#00ff7f";
          ctx.lineWidth = 2;
          ctx.beginPath();

          for (var i=0; i<data.length; ++i) {
            var p = data[i]["p"];
            var x = i * (width / (data.length - 1));
            var y = (1.0 - p) * (height-10) + 5;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
          }
          ctx.stroke();
        }

        Connections {
          target: telemetry
          function onUpdated() { plot.requestPaint(); }
        }
      }
    }

    Text { text: "Step 0 OK: UI + TelemetryBus + mock publisher"; color: "#808080" }
  }
}
