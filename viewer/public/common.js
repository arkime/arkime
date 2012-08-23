/*jshint
  browser: true, jquery: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true, globalstrict: true
*/
/*global startTime:true, stopTime:true, initialDisplayLength:true*/
"use strict";


//////////////////////////////////////////////////////////////////////////////////
// Utility Functions
//////////////////////////////////////////////////////////////////////////////////
function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

function dateString(seconds, sep) {
  var d = new Date(seconds*1000);
  return (d.getMonth()+1) +
         "/" +
         d.getDate() +
         sep +
         twoDigitString(d.getHours()) +
         ":" +
         twoDigitString(d.getMinutes()) +
         ":" +
         twoDigitString(d.getSeconds());
}

function ipString(ip) {
  return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) + '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
}

function parseUrlParams() {
  var urlParams = {};
  var e,
      a = /\+/g,  // Regex for replacing addition symbol with a space
      r = /([^&=]+)=?([^&]*)/g,
      d = function (s) { return decodeURIComponent(s.replace(a, " ")); },
      q = window.location.search.substring(1);

  while ((e = r.exec(q))) {
     urlParams[d(e[1])] = d(e[2]);
  }

  return urlParams;

}

// From http://stackoverflow.com/a/2901298
function numberWithCommas(x) {
  return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

// From http://datatables.net/plug-ins/api#fnLengthChange
$.fn.dataTableExt.oApi.fnLengthChange = function ( oSettings, iDisplay )
{
    oSettings._iDisplayLength = iDisplay;
    oSettings.oApi._fnCalculateEnd( oSettings );
      
    /* If we have space to show extra rows (backing up from the end point - then do so */
    if ( oSettings._iDisplayEnd === oSettings.aiDisplay.length )
    {
        oSettings._iDisplayStart = oSettings._iDisplayEnd - oSettings._iDisplayLength;
        if ( oSettings._iDisplayStart < 0 )
        {
            oSettings._iDisplayStart = 0;
        }
    }
      
    if ( oSettings._iDisplayLength === -1 )
    {
        oSettings._iDisplayStart = 0;
    }
      
    oSettings.oApi._fnDraw( oSettings );
      
    if ( oSettings.aanFeatures.l )
    {
        $('select', oSettings.aanFeatures.l).val( iDisplay );
    }
};

//From http://datatables.net/plug-ins/sorting
jQuery.extend( jQuery.fn.dataTableExt.oSort, {
    "formatted-num-pre": function ( a ) {
        a = (a==="-") ? 0 : a.replace( /[^\d\-\.]/g, "" );
        return parseFloat( a );
    },
 
    "formatted-num-asc": function ( a, b ) {
        return a - b;
    },
 
    "formatted-num-desc": function ( a, b ) {
        return b - a;
    }
} );

// From http://stackoverflow.com/a/8999941
(function ($, undefined) {
    $.fn.getCursorPosition = function() {
        var el = $(this).get(0);
        var pos = 0;
        if('selectionStart' in el) {
            pos = el.selectionStart;
        } else if('selection' in document) {
            el.focus();
            var Sel = document.selection.createRange();
            var SelLength = document.selection.createRange().text.length;
            Sel.moveStart('character', -el.value.length);
            pos = Sel.text.length - SelLength;
        }
        return pos;
    };
}(jQuery));

(function ($, undefined) {
    $.fn.getCurrentWord = function() {
      var val = $(this).val();
      var start, end;
      for (start = $(this).getCursorPosition(); start > 0; start--) {
        if (val[start-1] === " ") {
          break;
        }
      }

      for (end = $(this).getCursorPosition(); end < val.length; end++) {
        if (val[end] === " ") {
          break;
        }
      }

      return val.substring(start, end);
    };
}(jQuery));


function splitExpression(input ) {
  input = input.replace(/ /g, "");
  var output = [];
  var cur = "";

  for (var i = 0; i < input.length; i++) {
      if (/[)(]/.test(input[i])) {
        if (cur !== "") {
          output.push(cur);
        }
        output.push(input[i]);
        cur = "";
      } else if (cur === "") {
        cur += input[i];
      } else if (/[!&|=]/.test(cur)) {
        if (/[&|=]/.test(input[i])) {
          cur += input[i];
        } else {
          output.push(cur);
          cur = input[i];
        }
      } else if (/[!&|=]/.test(input[i])) {
        output.push(cur);
        cur = input[i];
      } else {
        cur += input[i];
      }
  }
  if (cur !== "") {
    output.push(cur);
  }
  return output;
}

//////////////////////////////////////////////////////////////////////////////////
// index Functions
//////////////////////////////////////////////////////////////////////////////////
function handleUrlParams() {
  var urlParams = parseUrlParams();

  if (urlParams.date) {
    $("#date").val(urlParams.date);
  } else {
    $("#date").val("");
  }

  if (urlParams.expression) {
    $("#expression").val(urlParams.expression);
  } else {
    $("dexpression").val("");
  }

  initialDisplayLength = urlParams.iDisplayLength || 100;

  if (urlParams.startTime && urlParams.startTime) {
    startTime = urlParams.startTime;
    stopTime = urlParams.stopTime;
    $("#date").val("-2");
  }

  return urlParams;
}

function addExpression (expression) {
  var val = $("#expression").val();
  if (val === "") {
    $("#expression").val(expression);
  } else {
    $("#expression").val(val + " && " + expression);
  }
  return false;
}

function addSessionIPStr (ip) {
  return addExpression("ip == " + ip);
}

function addSessionIP (ip) {
  return addSessionIPStr(ipString(ip));
}

function setSessionStartTime (t) {
  startTime = t;
  $("#date").val("-2");
  return false;
}

function setSessionStopTime (t) {
  stopTime = t;
  $("#date").val("-2");
  return false;
}

function drawGraph(graphData) {
  if (!graphData) {
    return;
  }

  var interval = 60*1000;
  var color = "#000000";
  var plot = $.plot(
  $("#sessionGraph"), [{
    data: graphData
  }], {
    series: {
      lines: {
        show: false,
        fill: true
      },
      bars: {
        show: true,
        fill: 1,
        barWidth: interval / 1.7
      },
      points: {
        show: false
      },
      color: color,
      shadowSize: 0
    },
    xaxis: {
      mode: "time",
      Atimeformat: "%H:%M:%S<br>%m/%d",
      label: "Datetime",
      color: "#000",
      tickFormatter: function(v, axis) {
        return dateString(v/1000, "<br>");
      }
    },
    yaxis: {
      min: 0,
      color: "#000"
    },
    selection: {
      mode: "x",
      color: '#000'
    },
    zoom : {
      interactive: false,
      trigger: "dblclick",
      amount: 2
    },
    pan : {
      interactive: false,
      cursor: "move",
      frameRate: 20
    },
    legend: {
      position: "nw"
    },
    grid: {
      backgroundColor: '#fff',
      borderWidth: 0,
      borderColor: '#000',
      color: "#ddd",
      hoverable: true,
      clickable: true
    }
  });

  // code from flot - add zoom out button
  $('<div class="button" style="right:20px;top:5px">zoom out</div>').appendTo($("#sessionGraph")).click(function (e) {
      e.preventDefault();
      plot.zoomOut();
  });

  function addArrow(dir, right, top, offset) {
      $('<img class="button" src="/flot-0.7/examples/arrow-' + dir + '.gif" style="right:' + right + 'px;top:' + top + 'px">').appendTo($("#sessionGraph")).click(function (e) {
        e.preventDefault();
        plot.pan(offset);
      });
  }

  addArrow('left', 115, 5, { left: -100 });
  addArrow('right', 85, 5, { left: 100 });
}

function buildParams() {
  var params = [];

  if ($("#date").val() === "-2") {
    params.push({name:'startTime', value:startTime});
    params.push({name:'stopTime', value:stopTime});
  } else if ($("#date").val()) {
    params.push({name:'date', value:$("#date").val()});
  }

  if ($("#expression").val()) {
    params.push({name:'expression', value:$("#expression").val()});
  }
  return params;
}

    // Code from Kibana
function showTooltip(x, y, contents) {
  $('<div id="tooltip">' + contents + '</div>').css({
    position: 'absolute',
    display: 'none',
    top: y - 20,
    left: x - 100,
    color: '#eee',
    padding: '3px',
    'font-size': '8pt',
    'background-color': '#000',
    border: '1px solid #000',
    'font-family': '"Verdana", Geneva, sans-serif'
  }).appendTo("body").fadeIn(200);
}

function setupSessionGraphBinds(sessionsTable) {
  // Code from Kibana
  var previousPoint = null;
  $("#sessionGraph").bind("plothover", function (event, pos, item) {
    $("#x").text(pos.x.toFixed(2));
    $("#y").text(pos.y.toFixed(2));

    if (item) {
      if (previousPoint !== item.dataIndex) {
        previousPoint = item.dataIndex;

        $("#tooltip").remove();
        var x = item.datapoint[0].toFixed(0),
          y = Math.round(item.datapoint[1]*100)/100;

        showTooltip(
          item.pageX, item.pageY, y + " at " + dateString(x/1000, "<br>")
        );
      }
    } else {
      $("#tooltip").remove();
      previousPoint = null;
    }
  });

  $('#sessionGraph').bind("plotselected", function (event, ranges) {
    startTime = Math.floor(ranges.xaxis.from/1000);
    stopTime = Math.floor(ranges.xaxis.to/1000);
    $("#date").val("-2");
    sessionsTable.fnDraw();
  });

  $("#sessionGraph").bind('plotpan plotzoom', function (event, plot) {
      var axes = plot.getAxes();
      startTime = Math.floor(axes.xaxis.min/1000)-1;
      stopTime = Math.ceil(axes.xaxis.max/1000)+1;
      $("#date").val("-2");
      sessionsTable.fnDraw();
  });
}

//////////////////////////////////////////////////////////////////////////////////
// users Functions
//////////////////////////////////////////////////////////////////////////////////
