#!/usr/bin/perl

use strict;
use warnings;

use Qt4;
use TetrixWindow;

sub main {
    my $app = Qt4::Application( \@ARGV );
    my $window = TetrixWindow();
    $window->show();
    srand (time ^ $$ ^ unpack "%L*", `ps axww | gzip -f`);
    exit $app->exec();
}

main();
