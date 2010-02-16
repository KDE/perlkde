package TabletApplication;

use strict;
use warnings;
use blib;

use Qt;
use Qt::isa qw( Qt::Application );

sub setCanvas {
    my ($canvas) = @_;
    this->{myCanvas} = $canvas;
}

sub myCanvas() {
    return this->{myCanvas};
}

# [0]
sub event {
    my ($event) = @_;
    if ($event->type() == Qt::Event::TabletEnterProximity() ||
        $event->type() == Qt::Event::TabletLeaveProximity()) {
        CAST( $event, 'Qt::TabletEvent' );
        this->myCanvas->setTabletDevice(
            $event->device());
        return 1;
    }
    $DB::single=1;
    Qt::_internal::setDebug(0xffffff);
    return this->SUPER::event($event);
}
# [0]

1;
