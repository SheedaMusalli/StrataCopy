import QtQuick
Canvas {
    id: graph
    property var points: appState.historyGraph.points
    property real progress: appState.progress
    onPointsChanged: requestPaint()
    onProgressChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx=getContext("2d");ctx.reset()
        const w=width,h=height
        ctx.fillStyle="#20272b";ctx.fillRect(0,0,w,h)
        ctx.strokeStyle="#354149";ctx.lineWidth=1
        for(let x=0;x<=w;x+=w/10){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke()}
        for(let y=0;y<=h;y+=h/4){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
        if(!points.length)return
        let maximum=10
        for(let i=0;i<points.length;i++)maximum=Math.max(maximum,points[i].write*1.25)
        const end=w*Math.max(0.01,Math.min(1,progress))
        ctx.fillStyle="#42616e";ctx.fillRect(0,0,end,h)
        ctx.beginPath();ctx.moveTo(0,h)
        for(let i=0;i<points.length;i++)ctx.lineTo(end*i/Math.max(1,points.length-1),h-Math.min(h-2,points[i].write/maximum*h))
        ctx.lineTo(end,h);ctx.closePath();ctx.fillStyle="#4cc8ec";ctx.fill()
    }
}
