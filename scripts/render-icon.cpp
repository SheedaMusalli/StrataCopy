#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    if(app.arguments().size()!=3)return 2;
    QSvgRenderer svg(app.arguments()[1]);if(!svg.isValid())return 3;
    QImage image(1024,1024,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    {QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);svg.render(&painter);}
    return image.save(app.arguments()[2])?0:4;
}
