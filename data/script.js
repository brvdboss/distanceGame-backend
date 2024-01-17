var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

var source = new EventSource('measurements');

var loglist = "";
const dataMap = new Map();
var liveChart;
let counter = 0;
const maxDataPoints = 500;

/**
 * Set up websocket and define callback methods
 */
function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage; // <-- add this line



}

function initEventSource() {
  if (!!window.EventSource) {
    source.addEventListener('open', function(e) {
      console.log("Events Connected");
    }, false);

    source.addEventListener('error', function(e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    }, false);

    source.addEventListener('message', function(e) {
      console.log("message", e.data);
    }, false);

    source.addEventListener('distance', function(e) {
      //console.log("time", e.timeStamp);
      //console.log("distance", e.data);

      updateData(e.data,e.timeStamp);
    }, false);
  }
}

function initChart() {
  const ctx = document.getElementById('liveChart').getContext('2d');
      liveChart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: Array.from({ length: maxDataPoints }, (_, i) => i), // X-axis labels (counter or time-based counter)
          datasets: [{
            label: 'Live Data',
            data: Array(maxDataPoints).fill(null), // Y-axis data points
            borderColor: 'rgb(75, 192, 192)',
            borderWidth: 2,
            fill: false,
            tension: 0.5,  // Set tension to 0 for straight line segments
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          scales: {
            x: {
              type: 'linear',
              position: 'bottom',
              min: 0,
              max: maxDataPoints - 1,  // Set the maximum value of the x-axis
            },
            y: {
              beginAtZero: true,
              suggestedMin: 0,  // Set the minimum value of the y-axis
              suggestedMax: 10000,  // Set the maximum value of the y-axis
            }
          },
          animation: false,  // Disable all animations
        }
      });
}

function onOpen(event) {
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}


function onMessage(event) {
  const input = JSON.parse(event.data);
  updateData(input.distance, counter++);
}

function updateData(val, ts) {
  //Update internal data structures
  document.getElementById("distance").innerHTML = val;
  document.getElementById("timestamp").innerHTML = ts;

    // Update chart data
    liveChart.data.labels.push(maxDataPoints+ts);
    liveChart.data.datasets[0].data.push(val);

    // Shift the x-axis labels and data points if the limit is reached
    if (liveChart.data.labels.length > maxDataPoints) {
      liveChart.data.labels.shift();
      liveChart.data.datasets[0].data.shift();

      // Update the x-axis scale dynamically
      liveChart.options.scales.x.min=ts-50*maxDataPoints;
      liveChart.options.scales.x.max=ts;
    }

    // Update the chart without animation
    liveChart.update();
  }





window.addEventListener('load', onLoad);

function onLoad(event) {
  initWebSocket();
  initChart();
  initEventSource();
  //initButton();
}

/**
 * In the scrollable field keep it always on the last line
 */
/*function scrollLog() {
  var objDiv = document.getElementById("log");
  objDiv.scrollTop = objDiv.scrollHeight;
}*/

/*
function initButton() {
  document.getElementById('button').addEventListener('click', toggle);
}*/
