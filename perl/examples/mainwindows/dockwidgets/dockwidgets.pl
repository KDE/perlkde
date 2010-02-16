#!/usr/bin/perl -w

use strict;
use blib;

use Qt;
use MainWindow;

sub main {
    my $app = Qt::Application();
    my $mainWin = MainWindow();
    $mainWin->show();
    exit $app->exec();
}

main();
