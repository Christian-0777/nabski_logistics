#pragma once

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

class ProfileImageLabel final : public QLabel
{
public:
    explicit ProfileImageLabel(bool circular, QWidget* parent = nullptr)
        : QLabel(parent), circular_(circular)
    {
    }

    void setImage(const QPixmap& image)
    {
        image_ = image;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (image_.isNull())
        {
            QLabel::paintEvent(event);
            return;
        }

        QLabel::paintEvent(event);
        const QRect target = circular_ ? rect().adjusted(2, 2, -2, -2)
                                       : rect().adjusted(1, 1, -1, -1);
        if (target.width() <= 0 || target.height() <= 0)
        {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if (circular_)
        {
            QPainterPath clip;
            clip.addEllipse(target);
            painter.setClipPath(clip);
        }
        const qreal targetAspect = static_cast<qreal>(target.width()) / target.height();
        const qreal imageAspect = static_cast<qreal>(image_.width()) / image_.height();
        QRect source = image_.rect();
        if (imageAspect > targetAspect)
        {
            const int sourceWidth =
                qBound(1, qRound(image_.height() * targetAspect), image_.width());
            source.setLeft((image_.width() - sourceWidth) / 2);
            source.setWidth(sourceWidth);
        }
        else
        {
            const int sourceHeight =
                qBound(1, qRound(image_.width() / targetAspect), image_.height());
            source.setTop((image_.height() - sourceHeight) / 2);
            source.setHeight(sourceHeight);
        }
        painter.drawPixmap(target, image_, source);
    }

private:
    QPixmap image_;
    bool circular_ = false;
};
