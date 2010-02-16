package MyApp;

use Test::More;

use Qt;
use Qt::isa('Qt::Application');
use Qt::slots
        foo => ['int'],
        baz => [];
use Qt::signals
        bar => ['int'];

sub NEW {
     shift->SUPER::NEW(@_);
     this->connect(this, SIGNAL 'bar(int)', SLOT 'foo(int)');
     this->connect(this, SIGNAL 'aboutToQuit()', SLOT 'baz()');
}

sub foo {
    # 1) testing correct inheritance of sig/slots
    is($_[0], 3, 'Correct inheritance of sig/slots');
}

sub baz {
    ok( 1 );
}     

sub coincoin {
    is( scalar @_, 2);
    is( ref(this), ' MySubApp');
}

1;

package MySubApp;

use Test::More;

use Qt;
use Qt::isa('MyApp');

sub NEW 
{
    shift->SUPER::NEW(@_);
    emit foo(3);
}

sub baz
{
   # 2) testing further inheritance of sig/slots
   ok( 1, 'Further inheritance of sig/slots' );
   # 3) testing Perl to Perl SUPER
   SUPER->baz();
   # 4) 5) 6) testing non-qualified enum calls vs. Perl method/static calls
   ok( eval { Qt::blue } );
   ok( !$@ );
   diag( $@ );
   coincoin('a','b');
}

1;

package main;

use Test::More tests => 7;

use Qt;
use MySubApp;

$a = 0;
$a = MySubApp(\@ARGV);

Qt::Timer::singleShot( 300, qApp, SLOT "quit()" );

exit qApp->exec;
