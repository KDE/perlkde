#!/usr/bin/perl -w

use strict;
use warnings;

package main;

use Qt4;
use GameBoard;

sub main {
    my $app = Qt4::Application( \@ARGV );
    my $widget = GameBoard();
    $widget->setGeometry(100, 100, 500, 355);
    $widget->show();
    return $app->exec();
} 

main();
