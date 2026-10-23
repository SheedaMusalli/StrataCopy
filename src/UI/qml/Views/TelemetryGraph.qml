import QtQuick
Canvas {
    id: graph
    property var points: appState.historyGraph.points
    onPointsChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx=getContext("2d");ctx.reset()
        const w=width,h=height-8
        ctx.strokeStyle="#1d293a";ctx.lineWidth=1
        for(let k=0;k<4;k++){const y=8+k*(h-8)/3;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
        if(points.length<2)return
        let maximum=10
        for(let i=0;i<points.length;i++)maximum=Math.max(maximum,points[i].read,points[i].write)
        const chart=(key,color,fill)=>{
            const span=Math.max(60,points.length-1)
            ctx.beginPath()
            for(let i=0;i<points.length;i++){
                const x=w-(points.length-1-i)*w/span,y=h-points[i][key]/maximum*(h-12)
                if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)
            }
            ctx.strokeStyle=color;ctx.lineWidth=1.8;ctx.stroke()
            ctx.lineTo(w,h);ctx.lineTo(w-(points.length-1)*w/span,h);ctx.closePath()
            const gradient=ctx.createLinearGradient(0,0,0,h);gradient.addColorStop(0,fill);gradient.addColorStop(1,"transparent");ctx.fillStyle=gradient;ctx.fill()
        }
        chart("read","#00dce8","#16465a");chart("write","#8785ff","#292443")
    }
}
