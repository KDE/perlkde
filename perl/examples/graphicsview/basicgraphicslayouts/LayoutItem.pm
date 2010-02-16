package LayoutItem;

use strict;
use warnings;
use Qt;
# [0]
use Qt::isa qw( Qt::GraphicsWidget );

sub pix() {
    return this->{pix};
}

# [0]
sub NEW
{
    my ($class, $parent) = @_;
    $class->SUPER::NEW( $parent );
    this->{pix} = Qt::Pixmap('images/block.png');
    # Do not allow a size smaller than the pixmap with two frames around it.
    this->setMinimumSize(Qt::SizeF(this->pix->size() + Qt::Size(12, 12)));
}
# [0]

# [1]
sub paint
{
    my ($painter) = @_;

    my $frame = Qt::RectF(Qt::PointF(0,0), this->geometry()->size());
    my $w = this->pix->width();
    my $h = this->pix->height();
    my @stops;
# [1]

# [2]
    # paint a background rect (with gradient)
    my $gradient = Qt::LinearGradient($frame->topLeft(), $frame->topLeft() + Qt::PointF(200,200));
    push @stops, Qt::GradientStop(0.0, Qt::Color(60, 60,  60));
    push @stops, Qt::GradientStop($frame->height()/2/$frame->height(), Qt::Color(102, 176, 54));

    push @stops, Qt::GradientStop(1.0, Qt::Color(215, 215, 215));
    $gradient->setStops(\@stops);
    $painter->setBrush(Qt::Brush($gradient));
    $painter->drawRoundedRect($frame, 10.0, 10.0);

    # paint a rect around the pixmap (with gradient)
    my $pixpos = $frame->center() - (Qt::PointF($w, $h)/2);
    my $innerFrame = Qt::RectF($pixpos, Qt::SizeF($w, $h));
    $innerFrame->adjust(-4, -4, +4, +4);
    $gradient->setStart($innerFrame->topLeft());
    $gradient->setFinalStop($innerFrame->bottomRight());
    @stops = ();
    push @stops, Qt::GradientStop(0.0, Qt::Color(215, 255, 200));
    push @stops, Qt::GradientStop(0.5, Qt::Color(102, 176, 54));
    push @stops, Qt::GradientStop(1.0, Qt::Color(0, 0,  0));
    $gradient->setStops(\@stops);
    $painter->setBrush(Qt::Brush($gradient));
    $painter->drawRoundedRect($innerFrame, 10.0, 10.0);
    $painter->drawPixmap($pixpos, this->pix);
}
# [2]

1;
