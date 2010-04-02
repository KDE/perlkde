package MainWindow;

use strict;
use warnings;

use QtCore4;
use KDEUi4;
use KIO4;
use Qt4::GlobalSpace qw( i18n );

use Qt4::isa qw( KDE::XmlGuiWindow );
use Qt4::slots
    newFile => [],
    openFile => [],
    saveFile => [],
    saveFileAs => [],
    saveFileAs => ['const QString &'];

sub NEW {
    my ($class, $parent) = @_;
    $class->SUPER::NEW( $parent );

    this->{textArea} = KDE::TextEdit();
    this->setCentralWidget(this->{textArea});

    this->setupActions();
}

sub setupActions
{
    my $clearAction = KDE::Action(this);
    $clearAction->setText(i18n('Clear'));
    $clearAction->setIcon(KDE::Icon('document-new'));
    $clearAction->setShortcut(Qt4::KeySequence('Ctrl+W'));
    this->actionCollection()->addAction('clear', $clearAction);
    this->connect($clearAction, SIGNAL 'triggered(bool)',
            this->{textArea}, SLOT 'clear()');

    KDE::StandardAction::quit(kapp, SLOT 'quit()',
            this->actionCollection());

    KDE::StandardAction::open(this, SLOT 'openFile()',
            this->actionCollection());

    KDE::StandardAction::save(this, SLOT 'saveFile()',
            actionCollection());

    KDE::StandardAction::saveAs(this, SLOT 'saveFileAs()',
            actionCollection());

    KDE::StandardAction::openNew(this, SLOT 'newFile()',
            actionCollection());

    this->setupGUI();
}

sub newFile
{
    this->{fileName} = undef;
    this->{textArea}->clear();
}

sub saveFileAs
{
    my ($outputFileName) = @_;
    if ( !defined $outputFileName ) {
        $outputFileName = KDE::FileDialog::getSaveFileName();
    }

    my $file = KDE::SaveFile($outputFileName);
    $file->open();

    my $outputByteArray = Qt4::ByteArray();
    $outputByteArray->append(this->{textArea}->toPlainText());
    $file->write($outputByteArray);
    $file->finalize();
    $file->close();

    this->{fileName} = $outputFileName;
}

sub saveFile
{
    this->saveFileAs(this->{fileName});
}

sub openFile
{
    my ( $inputFileName ) = @_;
    if ( !defined $inputFileName ) {
        $inputFileName = KDE::FileDialog::getOpenFileName();
    }
    $inputFileName = $inputFileName ? $inputFileName : '';

    my $tmpFile;
    if(KDE::IO::NetAccess::download(KDE::Url($inputFileName), $tmpFile,
                this))
    {
        my $file = Qt4::File($tmpFile);
        $file->open(Qt4::IODevice::ReadOnly());
        this->{textArea}->setPlainText(Qt4::TextStream($file)->readAll());
        this->{fileName} = $inputFileName;

        KDE::IO::NetAccess::removeTempFile($tmpFile);
    }
    else
    {
        KDE::MessageBox::error(this,
                KDE::IO::NetAccess::lastErrorString());
    }

}

1;
