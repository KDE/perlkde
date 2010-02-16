package Qt::signals;
use Carp;
#
# Proposed usage:
#
# use Qt::signals changeSomething => ['int'];
#

use Qt;

sub import {
    no strict 'refs';
    my $self = shift;
    croak "Odd number of arguments in signal declaration" if @_%2;
    my $caller = $self eq 'Qt::signals' ? (caller)[0] : $self;
    my(%signals) = @_;
    my $meta = \%{ $caller . '::META' };

    Qt::_internal::installqt_metacall( $caller ) unless defined &{$caller."::qt_metacall"};
    foreach my $signalname ( keys %signals ) {
        # Build the signature for this signal
        my $signature = join '', ("$signalname(", join(',', @{$signals{$signalname}}), ')');

        # Normalize the signature, might not be necessary
        $signature = Qt::QMetaObject::normalizedSignature(
           $signature )->data();

        my $signal = {
            name => $signalname,
            signature => $signature,
        };

        push @{$meta->{signals}}, $signal;
        Qt::_internal::installsignal("$caller\::$signalname") unless defined &{ "$caller\::$signalname" };
    }
}

1;
