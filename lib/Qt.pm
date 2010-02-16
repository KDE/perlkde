package Qt::base;

use strict;
use warnings;

sub new {
    # Any direct calls to the 'NEW' function will bypass this code.  It's
    # called that way in subclass constructors, thus setting the 'this' value
    # for that package.

    # Store whatever current 'this' value we've got
    my $packageThis = Qt::this();
    # Create the object, overwriting the 'this' value
    shift->NEW(@_);
    # Get the return value
    my $ret = Qt::this();
    # Restore package's this
    Qt::_internal::setThis($packageThis);
    # Give back the new value
    return $ret;
}

package Qt::enum::_overload;

use strict;

no strict 'refs';

use overload
    "fallback" => 1,
    "==" => "Qt::enum::_overload::op_equal",
    "+"  => "Qt::enum::_overload::op_plus",
    "|"  => "Qt::enum::_overload::op_or";

sub op_equal {
    if( ref $_[0] ) {
        if( ref $_[1] ) {
            return 1 if ${$_[0]} == ${$_[1]};
            return 0;
        }
        else {
            return 1 if ${$_[0]} == $_[1];
            return 0;
        }
    }
    else {
        return 1 if $_[0] == ${$_[1]};
        return 0;
    }
    # Never have to check for both not being references.  If neither is a ref,
    # this function will never be called.
}

sub op_plus {
    return bless( \(${$_[0]} + ${$_[1]}), ref $_[0] );
}

sub op_or {
    return bless( \(${$_[0]} | ${$_[1]}), ref $_[0] );
}

package Qt::_internal;

use strict;
use warnings;

# These 2 hashes provide lookups from a perl package name to a smoke
# classid, and vice versa
our %package2classId;
our %classId2package;

# This hash stores integer pointer address->perl SV association.  Used for
# overriding virtual functions, where all you have as an input is a void* to
# the object who's method is being called.  Made visible here for debugging
# purposes.
our %pointer_map;

sub argmatch {
    my ( $methodIds, $args, $argNum ) = @_;
    my %match;

    my $argType = getSVt( $args->[$argNum] );

               #index into methodId array
    foreach my $methodIdIdx ( 0..$#$methodIds ) {
        my $methodId = $$methodIds[$methodIdIdx];
        my $typeName = getTypeNameOfArg( $methodId, $argNum );
        #ints and bools
        if ( $argType eq 'i' ) {
            if( $typeName =~ m/^(?:bool|(?:(?:un)?signed )?(?:int|long)|uint)[*&]?$/ ) {
                $match{$methodId} = [0,$methodIdIdx];
            }
        }
        # floats and doubles
        elsif ( $argType eq 'n' ) {
            if( $typeName =~ m/^(?:float|double)$/ ) {
                $match{$methodId} = [0,$methodIdIdx];
            }
        }
        # enums
        elsif ( $argType eq 'e' ) {
            if( $typeName eq ref $args->[$argNum] ) {
                $match{$methodId} = [0,$methodIdIdx];
            }
        }
        # strings
        elsif ( $argType eq 's' ) {
            print "String test in method name resolution not implemented\n";
            return ();
        }
        # arrays
        elsif ( $argType eq 'a' ) {
            print "Array test in method name resolution not implemented\n";
            return ();
        }
        elsif ( $argType eq 'r' or $argType eq 'U' ) {
            $match{$methodId} = [0,$methodIdIdx];
        }
        # objects
        else {
            $typeName =~ s/^const\s+(\w*)[&*]$/$1/g;
            my $isa = classIsa( $argType, $typeName );
            if ( $isa != -1 ) {
                $match{$methodId} = [-$isa, $methodIdIdx];
            }
        }
    }
    return sort { $match{$b}[0] <=> $match{$a}[0] or $match{$a}[1] <=> $match{$b}[1] } keys %match;
}

# Args: @_: the args to the method being called
#       $classname: the c++ class being called
#       $methodname: the c++ method name being called
#       $classId: the smoke class Id of $classname
# Returns: A disambiguated method id
# Desc: Examines the arguments of the method call to build a method signature.
#       From that signature, it determines the appropriate method id.
sub do_autoload {
    my $classname = pop;
    my $methodname = pop;
    my $classId = pop;

    # Loop over the arguments to determine the type of args
    my @mungedMethods = ( $methodname );
    foreach my $arg ( @_ ) {
        if (!defined $arg) {
            # An undefined value requires a search for each type of argument
            @mungedMethods = map { $_ . '#', $_ . '?', $_ . '$' } @mungedMethods;
        } elsif(isObject($arg)) {
            @mungedMethods = map { $_ . '#' } @mungedMethods;
        } elsif((ref $arg) =~ m/HASH|ARRAY/) {
            @mungedMethods = map { $_ . '?' } @mungedMethods;
        } else {
            @mungedMethods = map { $_ . '$' } @mungedMethods;
        }
    }
    my @methodIds = map { findMethod( $classname, $_ ) } @mungedMethods;

    # If we got more than 1 method id, resolve it
    if (@methodIds > 1) {
        my $count = scalar @_;
        foreach my $argNum (0..$count-1) {
            my @matching = argmatch( \@methodIds, \@_, $argNum );
            @methodIds = @matching if @matching;
        }

        # If we still have more than 1 match, die.
        if ( @methodIds > 1 ) {
            # A constructor call will be 4 levels deep on the stack, everything
            # else will be 2
            my $stackDepth = ( $methodname eq $classname ) ? 4 : 2;
            die "--- Ambiguous method ${classname}::$methodname " .
                "called at " . (caller($stackDepth))[1] .
                " line " . (caller($stackDepth))[2] . "\n";
        }
    }
    elsif ( @methodIds == 1 and @_ ) {
        # We have one match and arguments.  We need to make sure our input
        # arguments match what the method is expecting.  Clear methodIds if
        # args don't match
        if (!objmatch($methodIds[0], \@_)) {
            @methodIds = ();
            die "--- Objects didn't match signature in call to $methodname\n";
        }
    }

    if ( !@methodIds ) {
        die "--- No method found in lookup for $classname\::$methodname\n";
    }

    return $methodIds[0];
}

sub getMetaObject {
    no strict 'refs';
    my $class = shift;
    my $meta = \%{ $class . '::META' };

    # If no signals/slots/properties have been added since the last time this
    # was asked for, return the saved one.
    return $meta->{object} if $meta->{object} and !$meta->{changed};

    # Get the super class's meta object for sig/slot inheritance
    # Look up through ISA to find it
    my $parentMeta = undef;
    my $parentClassId;

    # This seems wrong, it won't work with multiple inheritance
    my $parentClass = (@{$class."::ISA"})[0]; 
    if( !$package2classId{$parentClass} ) {
        # The parent class is a custom Perl class whose metaObject was
        # constructed at runtime, so we can get it's metaObject from here.
        $parentMeta = getMetaObject( $parentClass );
    }
    else {
        $parentClassId = $package2classId{$parentClass};
    }

    # Generate data to create the meta object
    my( $stringdata, $data ) = makeMetaData( $class );
    $meta->{object} = Qt::_internal::make_metaObject(
        $parentClassId,
        $parentMeta,
        $stringdata,
        $data );

    $meta->{changed} = 0;
    return $meta->{object};
}

sub init_class {
    no strict 'refs';

    my ($cxxClassName) = @_;

    my $perlClassName = normalize_classname($cxxClassName);
    my $classId = idClass($cxxClassName);

    # Save the association between this perl package and the cxx classId.
    $package2classId{$perlClassName} = $classId;
    $classId2package{$classId} = $perlClassName;

    # Define the inheritance array for this class.
    my @isa = getIsa($classId);

    # We want the isa array to be the names of perl packages, not c++ class
    # names
    foreach my $super ( @isa ) {
        $super = normalize_classname($super);
    }

    # The root of the tree will be Qt::base, so a call to
    # $className::new() redirects there.
    @isa = ('Qt::base') unless @isa;
    @{ "$perlClassName\::ISA" } = @isa;

    # Define the $perlClassName::_UTOLOAD function, which always redirects to
    # XS_AUTOLOAD in Qt.xs
    installautoload($perlClassName);
    installautoload(" $perlClassName");
    {
        # Putting this in one package gives XS_AUTOLOAD one spot to look for
        # the autoload variable
        package Qt::AutoLoad;
        my $closure = \&{ "$perlClassName\::_UTOLOAD" };
        *{ $perlClassName . "::AUTOLOAD" } = sub{ &$closure };
        $closure = \&{ " $perlClassName\::_UTOLOAD" };
        *{ " $perlClassName\::AUTOLOAD" } = sub{ &$closure };
    }

    *{ "$perlClassName\::NEW" } = sub {
        # Removes $perlClassName from the front of @_
        my $perlClassName = shift;
        $Qt::AutoLoad::AUTOLOAD = "$perlClassName\::$cxxClassName";
        my $_utoload = "$perlClassName\::_UTOLOAD";
        {
            no warnings;
            setThis( bless &$_utoload, " $perlClassName" );
        }
    } unless defined &{"$perlClassName\::NEW"};

    # Make the constructor subroutine
    *{ $perlClassName } = sub {
        # Adds $perlClassName to the front of @_
        $perlClassName->new(@_);
    } unless defined &{ $perlClassName };
}

# Args: none
# Returns: none
# Desc: sets up each class
sub init {
    my $classes = getClassList();
    foreach my $cxxClassName (@$classes) {
        init_class($cxxClassName);
    }

    no strict 'refs';
    my $enums = getEnumList();
    foreach my $enumName (@$enums) {
        $enumName =~ s/^const //;
        @{"${enumName}::ISA"} = ('Qt::enum::_overload');
    }
}

sub makeMetaData {
    no strict 'refs';
    my ( $classname ) = @_;
    my $meta = \%{ $classname . '::META' };
    my $classinfos = $meta->{classinfos};
    my $dbus = $meta->{dbus};
    my $signals = $meta->{signals};
    my $slots = $meta->{slots};

    @$signals = () if !defined @$signals;
    @$slots = () if !defined @$slots;

    # Each entry in 'stringdata' corresponds to a string in the
    # qt_meta_stringdata_<classname> structure.

    #
    # From the enum MethodFlags in qt-copy/src/tools/moc/generator.cpp
    #
    my $AccessPrivate = 0x00;
    my $AccessProtected = 0x01;
    my $AccessPublic = 0x02;
    my $MethodMethod = 0x00;
    my $MethodSignal = 0x04;
    my $MethodSlot = 0x08;
    my $MethodCompatibility = 0x10;
    my $MethodCloned = 0x20;
    my $MethodScriptable = 0x40;

    my $data = [1,               #revision
                0,               #str index of classname
                0, 0,            #don't have classinfo
                scalar @$signals + scalar @$slots, #number of sig/slots
                10,              #do have methods
                0, 0,            #no properties
                0, 0,            #no enums/sets
    ];

    my $stringdata = "$classname\0\0";
    my $nullposition = length( $stringdata ) - 1;

    # Build the stringdata string, storing the indexes in data
    foreach my $signal ( @$signals ) {
        my $curPosition = length $stringdata;

        # Add this signal to the stringdata
        $stringdata .= $signal->{signature} . "\0" ;

        push @$data, $curPosition; #signature
        push @$data, $nullposition; #parameter names
        push @$data, $nullposition; #return type, void
        push @$data, $nullposition; #tag
        push @$data, $MethodSignal | $AccessProtected; # flags
    }

    foreach my $slot ( @$slots ) {
        my $curPosition = length $stringdata;

        # Add this slot to the stringdata
        $stringdata .= $slot->{signature} . "\0" ;

        push @$data, $curPosition; #signature
        push @$data, $nullposition; #parameter names
        push @$data, $nullposition; #return type, void
        push @$data, $nullposition; #tag
        push @$data, $MethodSlot | $AccessPublic; # flags
    }

    push @$data, 0; #eod

    return ($stringdata, $data);
}

# Args: $cxxClassName: the name of a Qt class
# Returns: The name of the associated perl package
# Desc: Given a c++ class name, determine the perl package name
sub normalize_classname {
    my ( $cxxClassName ) = @_;

    # Don't modify the 'Qt' class
    return $cxxClassName if $cxxClassName eq 'Qt';

    my $perlClassName = $cxxClassName;

    if ($cxxClassName =~ m/^Q3/) {
        # Prepend Qt3:: if this is a Qt3 support class
        $perlClassName =~ s/^Q3(?=[A-Z])/Qt3::/;
    }
    elsif ($cxxClassName =~ m/^Q/) {
        # Only prepend Qt:: if the name starts with Q and is followed by
        # an uppercase letter
        $perlClassName =~ s/^Q(?=[A-Z])/Qt::/;
    }

    return $perlClassName;
}

sub objmatch {
    my ( $methodname, $args ) = @_;
    foreach my $i ( 0..$#$args ) {
        # Compare our actual args to what the method expects
        my $argtype = getSVt($$args[$i]);

        # argtype will be only 1 char if it is not an object. If that's the
        # case, don't do any checks.
        next if length $argtype == 1;

        my $typename = getTypeNameOfArg( $methodname, $i );

        # We don't care about const or [&*]
        $typename =~ s/^const\s+//;
        $typename =~ s/(?<=\w)[&*]$//g;

        return 0 if classIsa($argtype, $typename) == -1;
    }
    return 1;
}

sub Qt::Application::NEW {
    my $class = shift;
    my $argv = shift;
    unshift @$argv, $0;
    my $count = scalar @$argv;
    my $retval = Qt::Application::QApplication( $count, $argv );
    bless( $retval, " $class" );
    setThis( $retval );
    setQApp( $retval );
    shift @$argv;
}

package Qt;

use 5.008006;
use strict;
use warnings;

require Exporter;
require XSLoader;
use Devel::Peek;

our $VERSION = '0.01';

our @EXPORT = qw( SIGNAL SLOT emit CAST qApp );

XSLoader::load('Qt', $VERSION);

Qt::_internal::init();

sub SIGNAL ($) { '2' . $_[0] }
sub SLOT ($) { '1' . $_[0] }
sub emit (@) { pop @_ }
sub CAST ($$) {
    my( $var, $class ) = @_;
    if( ref $var ) {
        return bless( $var, $class );
    }
    else {
        return bless( \$var, $class );
    }
}

sub import { goto &Exporter::import }

# Called in the DESTROY method for all QObjects to see if they still have a
# parent, and avoid deleting them if they do.
sub Qt::Object::ON_DESTROY {
    package Qt::_internal;
    my $parent = Qt::this()->parent;
    if( $parent ) {
        my $ptr = sv_to_ptr(Qt::this());
        ${ $parent->{'hidden children'} }{ $ptr } = Qt::this();
        Qt::this()->{'has been hidden'} = 1;
        return 1;
    }
    return 0;
}

# Never save a QApplication from destruction
sub Qt::Application::ON_DESTROY {
    return 0;
}

1;

=begin

=head1 NAME

Qt - Perl bindings for the Qt version 4 library

=head1 SYNOPSIS

  use Qt;

=head1 DESCRIPTION

This module is a port of the PerlQt3 package to work with Qt version 4.

=head2 EXPORT

None by default.

=head1 SEE ALSO

The existing Qt documentation is very complete.  Use it for your reference.

Get the project's current version at http://code.google.com/p/perlqt4/

=head1 AUTHOR

Chris Burel, E<lt>chrisburel@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2008 by Chris Burel

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.8 or,
at your option, any later version of Perl 5 you may have available.

=cut
