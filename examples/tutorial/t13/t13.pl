#!/usr/local/bin/perl -w

use strict;
use warnings;
use blib;

package main;

use Qt;
use GameBoard;

sub main {
    my $app = Qt::Application( \@ARGV );
    my $widget = GameBoard();
    $widget->setGeometry(100, 100, 500, 355);
    $widget->show();
    return $app->exec();
} 

main();
