#include "ContactView.hpp"
#include "ui_ContactView.h"
#include "AddressBookModel.hpp"
#include "public_key_address.hpp"

#include <KeyhoteeMainWindow.hpp>
#include <bts/application.hpp>
#include <bts/address.hpp>

#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

#include <QtWebKitWidgets/QWebFrame>
#include <QToolBar>
#include <QMessageBox>
#include <QToolButton>

extern bool gMiningIsPossible;

bool ContactView::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) 
  {
     QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
     switch(key_event->key()) 
     {
        case Qt::Key_Enter:
        case Qt::Key_Return:
           sendChatMessage();
           return true;
        default:
           break;
     }
  }
  return QObject::eventFilter(object, event);
}

bool ContactView::isChatSelected()
{
    return (ui->contact_pages->currentWidget() == ui->chat_page);
}


void ContactView::sendChatMessage()
{
    auto msg = ui->chat_input->toPlainText();
    if( msg.size() != 0 )
    {
        auto app = bts::application::instance();
        auto profile = app->get_profile();
        auto  idents = profile->identities();
        bts::bitchat::private_text_message text_msg( msg.toStdString() );
        if( idents.size() )
        {
           fc::ecc::private_key my_priv_key = profile->get_keychain().get_identity_key( idents[0].dac_id );
           app->send_text_message( text_msg, _current_contact.public_key, my_priv_key );
           appendChatMessage( "me", msg );
        }

        ui->chat_input->setPlainText(QString());
    }
}
void ContactView::appendChatMessage( const QString& from, const QString& msg, const QDateTime& date_time )
{ //DLNFIX2 improve formatting later
    wlog( "append... ${msg}", ("msg",msg.toStdString() ) );
    QString formatted_msg = date_time.toString("hh:mm ap") + " "+ from + ": " + msg;
    #if 1
    QColor color;
    if (from == "me")
      color = "grey";
    else
      color = "black";
    ui->chat_conversation->setTextColor(color);
    ui->chat_conversation->append(formatted_msg);
    #else //this doesn't start new paragraphs, probably not worth spending
    //time on as we'll like junk in favor of somethng else later
    QTextCursor text_cursor = ui->chat_conversation->textCursor();
    QString colorName = (from == "me") ? "grey" : "black";
    formatted_msg = QString("<font color=\"%1\">%2</font>").arg(colorName).arg(formatted_msg);
    ui->chat_conversation->insertHtml(formatted_msg);
    cursor.movePosition(QTextCursor::NextBlock);
    ui->chat_conversation->setTextCursor(text_cursor);
    #endif
}


ContactView::ContactView( QWidget* parent )
: QWidget(parent),
  ui( new Ui::ContactView() )
{
   _address_book = nullptr;
   _addingNewContact = false;
   ui->setupUi(this);   
   setModyfied (false);
   message_tools = new QToolBar( ui->toolbar_container ); 
   QGridLayout* grid_layout = new QGridLayout(ui->toolbar_container);
   grid_layout->setContentsMargins( 0,0,0,0);
   grid_layout->setSpacing(0);
   ui->toolbar_container->setLayout(grid_layout);
   grid_layout->addWidget(message_tools,0,0);
   
   send_mail = new QAction( QIcon( ":/images/128x128/send_mail.png"), tr( "Mail"), this );
   edit_contact = new QAction( QIcon(":/images/read-icon.png"), tr( "Edit (need new icon)"), this );
   share_contact = new QAction( QIcon(":/images/read-icon.png"), tr( "Share (need new icon)"), this );
   request_contact = new QAction( QIcon(":/images/read-icon.png"), tr( "Request contact (need new icon)"), this );
   save_contact = new QAction( QIcon(":/images/read-icon.png"), tr( "Save (need new icon)"), this );
   cancel_edit_contact = new QAction( QIcon(":/images/read-icon.png"), tr( "Discard changes (need new icon)"), this );
   
   message_tools->addAction( send_mail );
   message_tools->addAction( save_contact );
   message_tools->addAction( edit_contact );
   message_tools->addAction( share_contact );
   message_tools->addAction( request_contact );
   message_tools->addAction( cancel_edit_contact );

   //ui->chat_conversation->setHtml( "<html><head></head><body>Hello World<br/></body></html>" );
   connect( save_contact, &QAction::triggered, this, &ContactView::onSave );
   connect( cancel_edit_contact, &QAction::triggered, this, &ContactView::onCancel);
   connect( edit_contact, &QAction::triggered, this, &ContactView::onEdit );
   connect( send_mail, &QAction::triggered, this, &ContactView::onMail);
   connect( share_contact, &QAction::triggered, this, &ContactView::onShareContact);
   connect( request_contact, &QAction::triggered, this, &ContactView::onRequestContact);

   connect( ui->firstname, &QLineEdit::textChanged, this, &ContactView::firstNameChanged );
   connect( ui->lastname, &QLineEdit::textChanged, this, &ContactView::lastNameChanged );
   connect( ui->id_edit, &QLineEdit::textChanged, this, &ContactView::keyhoteeIdChanged );
   connect( ui->id_edit, &QLineEdit::textEdited, this, &ContactView::keyhoteeIdEdited );
   connect( ui->public_key, &QLineEdit::textEdited, this, &ContactView::publicKeyEdited );
   connect( ui->public_key, &QLineEdit::textChanged, this, &ContactView::publicKeyChanged );
   connect( ui->privacy_comboBox, &QComboBox::editTextChanged, this, &ContactView::privacyLevelChanged );   
   connect( ui->email, &QLineEdit::textChanged, this, &ContactView::emailChanged );
   connect( ui->phone, &QLineEdit::textChanged, this, &ContactView::phoneChanged );
   connect( ui->notes, &QPlainTextEdit::textChanged, this, &ContactView::notesChanged );
   connect( ui->public_key_to_clipboard, &QToolButton::clicked, this, &ContactView::onPublicKeyToClipboard );
   connect( ui->contact_pages, &QTabWidget::currentChanged, this, &ContactView::onTabChanged );

   keyEdit (false);
   ui->chat_input->installEventFilter(this);

   setContact( Contact() );

}

void ContactView::onEdit()
{   
   setAddingNewContact (false); //editing   
   keyEdit (true); //and set focus on the first field   
   ui->contact_pages->setCurrentIndex (info);
   ui->info_stack->setCurrentWidget(ui->info_edit);   
}

void ContactView::onSave()
{ try {
    _current_contact.first_name     = ui->firstname->text().toStdString();
    _current_contact.last_name      = ui->lastname->text().toStdString();
    _current_contact.dac_id_string  = ui->id_edit->text().toStdString();
    if( _current_record )
    {
       //_current_contact.bit_id_hash = _current_record->name_hash;
       if( !_current_contact.public_key.valid() )
       {
            _current_contact.public_key = _current_record->active_key;
            FC_ASSERT( _current_contact.public_key.valid() );
       }
       // TODO: lookup block id / timestamp that registered this ID
       // _current_contact.known_since.setMSecsSinceEpoch( );
    }
    else if( !_current_record ) /// note: user is entering manual public key
    {
       elog( "!current record??\n" );
       /*
       if( _current_contact.known_since == QDateTime() )
       {
           _current_contact.known_since = QDateTime::currentDateTime();
       }
       */
       std::string enteredPKey = ui->public_key->text().toStdString();
       if(enteredPKey.empty() == false)
         {
         assert(public_key_address::is_valid(enteredPKey) && "Some bug in control validator");
         public_key_address key_address(enteredPKey);
         _current_contact.public_key = key_address.key;
         }
    }
    _current_contact.privacy_setting = bts::addressbook::secret_contact;

    _address_book->storeContact( _current_contact );

    //DLNFIX
    #if 1
    keyEdit (false);
    ui->contact_pages->setCurrentIndex (info);
    #else
    setContact(_current_contact,ContactView::info);
    #endif
} FC_RETHROW_EXCEPTIONS( warn, "onSave" ) }

void ContactView::onCancel()
{
   if (isAddingNewContact()) {
      QMessageBox::warning(this, tr("Warning"), tr("Fix - Hide view"));
   }
   else //editing contact
   {
      keyEdit (false);
      ui->firstname->setText( _current_contact.first_name.c_str() );
      ui->lastname->setText( _current_contact.last_name.c_str() );
      ui->id_edit->setText( _current_contact.dac_id_string.c_str() );
      updateNameLabel();
   }
}

void ContactView::onChat()
{
   ui->contact_pages->setCurrentIndex (chat);
   //clear unread message count on display of chat window
   //DLNFIX maybe getMainWindow can be removed via some connect magic or similar observer notification?
   ContactGui* contact_gui = GetKeyhoteeWindow()->getContactGui(_current_contact.wallet_index);
   if (contact_gui)
      contact_gui->setUnreadMsgCount(0);
   ui->chat_input->setFocus();
}

void ContactView::onInfo()
{   
   keyEdit (false);
   ui->contact_pages->setCurrentIndex (info);
}

void ContactView::onMail()
{
    GetKeyhoteeWindow()->newMailMessageTo(_current_contact.wallet_index);
}


void ContactView::onShareContact()
{
   QMessageBox::warning(this, tr("Warning"), tr("Not supported"));
}

void ContactView::onRequestContact()
{
   QMessageBox::warning(this, tr("Warning"), tr("Not supported"));
}

ContactView::~ContactView()
{
}

void ContactView::setContact( const Contact& current_contact)
{ try {
    _current_contact = current_contact;
    bool has_null_public_key = _current_contact.public_key == fc::ecc::public_key_data();
    if ( has_null_public_key )
    {
        elog( "********* null public key!" );
        save_contact->setEnabled (false);
        ui->contact_pages->setCurrentIndex (info);

        if (gMiningIsPossible)
           ui->id_status->setText( tr( "Please provide a valid ID or public key" ) );
        else
        {
           ui->id_status->setText( tr( "Public Key Only Mode" ) );
        }

        if( _current_contact.first_name == std::string() && _current_contact.last_name == std::string() )
        {
            ui->id_edit->setText( QString() );
            ui->public_key->setText( QString() );
            //keyhoteeIds don't function when mining is not possible
            if (!gMiningIsPossible)
               ui->id_edit->setEnabled(false);
        }
    }
    else
    {
        /// note: you cannot change the id of a contact once it has been
        /// set... you must create a new contact anytime their public key
        /// changes.
        ui->id_edit->setEnabled(false);
        save_contact->setEnabled (true);
        onInfo();
        /** TODO... restore this kind of check
        if( _current_contact.bit_id_public_key != _current_contact.public_key  )
        {
            ui->id_status->setText( 
                    tr( "Warning! Keyhotee ID %1 no longer matches known public key!" ).arg(_current_contact.bit_id) );
        }
        */
    }

    ui->firstname->setText( _current_contact.first_name.c_str() );
    ui->lastname->setText( _current_contact.last_name.c_str() );
   // ui->email->setText( _current_contact.email_address );
   // ui->phone->setText( _current_contact.phone_number );
    std::string public_key_string = public_key_address( _current_contact.public_key );
    ui->public_key->setText( public_key_string.c_str() );
    ui->id_edit->setText( _current_contact.dac_id_string.c_str() );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

Contact ContactView::getContact()const
{
    return _current_contact;
}

void ContactView::firstNameChanged( const QString& /*name*/ )
{
    updateNameLabel();
    setModyfied ();
}

void ContactView::keyhoteeIdChanged( const QString& /*name*/ )
{
    updateNameLabel();
    setModyfied ();
}

void ContactView::updateNameLabel()
{
   /*auto full_name = ui->firstname->text() + " " + ui->lastname->text();
   QString dac_id = ui->id_edit->text();
   if (dac_id != QString())
     full_name += "(" + dac_id + ")";
   if( full_name != " " )
   {
       ui->name_label->setText(full_name);
   }
   else
   {
       ui->name_label->setText(tr( "New Contact" ));
   }*/
}

void ContactView::lastNameChanged( const QString& /*name*/ )
{
   updateNameLabel();
   setModyfied ();
}

/*****************  Algorithm for handling keyhoteeId, keyhoteeeId status, and public key fields 
Notes:
If gMiningIsPossible,
  We can lookup a public key from a kehoteeId
  We can validate a public key is registered, 
    but we can't lookup the associated keyhoteeId, only a hash of the keyhoteeId

Some choices in Display Status for id not found on block chain: Available, Unable to find, Not registered

*** When creating new identity (this is for later implementation and some details may change):

Note: Public key field is not editable (only keyhotee-generated public keys are allowed as they must be tied to wallet)

If gMiningPossible,
  Display Mining Effort combo box: 
    options: Let Expire, Renew Quarterly, Renew Monthly, Renew Weekly, Renew Daily, Max Effort

  If keyhoteeId changed, lookup id and report status
    Display status: Not Available (red), Available (black), Registered To Me (green), Registering (yellow)
        (If keyhoteeId is not registered and mining effort is not 'Let Expire', then status is "Registering')
    OR:
    Display status: Registered (red), Not Registered (black), Registered To Me (green), Registering (yellow)
        (If keyhoteeId is not registered and mining effort is not 'Let Expire', then status is "Registering')
    Generate new public key based on keyhoteeId change and display it

 
If not gMiningPossible,
  Hide Mining Effort combo box: 

  If keyhoteeId changed, just keep it
    Generate new public key based on keyhoteeId change and display it

*** When adding a contact:

If gMiningPossible,
  If keyhoteeId changed, lookup id and report status
    Display status: Registered (green), Unable to find (red)
    if keyhoteeId registered in block chain, set public key field to display it
    if keyhoteeId field not registered in block chain, clear public key field
    enable save if valid public key or disable save

  If public key changed, validate it
    if public key is registered, change keyhotee field to ********
    if public key is not registered, clear keyhotee id field
    enable save if valid public key or disable save

If not gMiningPossible,
  Disable keyhoteeId field
  If public key changed, validate it
    enable save if valid public key or disable save

*** When editing a contact:

If gMiningPossible,
  Public key is not editable
  if keyhotee set, set as not editable
  If keyhoteeId blank, lookup id and report status
    Display status: Matches (green)
                    Mismatch (red)
  Doesn't save keyhoteeId on mismatch (i.e. field data isn't transferred to the contact record)          

If not gMiningPossible,
  Public key is not editable
  KeyhoteeId is not editable

*/
void ContactView::keyhoteeIdEdited( const QString& id )
{
   /** TODO
    if( is_address( id ) )
   {
      _complete = true;
   }
   else
   */
   {
      _last_validate = fc::time_point::now();
      ui->id_status->setText( tr( "Looking up id..." ) );
      fc::async( [=](){ 
          fc::usleep( fc::microseconds(500*1000) );
          if( fc::time_point::now() > (_last_validate + fc::microseconds(500*1000)) )
          {
             lookupId();
          }
      } );
   }
   updateNameLabel();
}

//implement real version and put in bitshares or fc (probably should be in fc)
bool is_registered_public_key(std::string public_key_string) 
{ 
   return false; //(public_key_string == "invictus");
} 

void ContactView::publicKeyEdited( const QString& public_key_string )
{
   ui->id_edit->clear(); //clear keyhotee id field
   if (gMiningIsPossible)
   {
      lookupPublicKey();
   }
   //check for validly hashed public key and enable/disable save button accordingly
   bool public_key_is_valid = public_key_address::is_valid(public_key_string.toStdString());
   if (public_key_is_valid)
   {
      ui->id_status->setText( tr("Public Key Only Mode: valid key") );
      ui->id_status->setStyleSheet("QLabel { color : green; }");
   }
   else
   {
      ui->id_status->setText( tr("Public Key Only Mode: not a valid key") );
      ui->id_status->setStyleSheet("QLabel { color : red; }");
   }
   save_contact->setEnabled(public_key_is_valid);
}

void ContactView::lookupId()
{
   try {
       auto current_id = ui->id_edit->text().toStdString();
       if( current_id.empty() )
       {
            ui->id_status->setText( QString() );
            save_contact->setEnabled(false);
            return;
       }
       _current_record = bts::application::instance()->lookup_name( current_id );
       if( _current_record )
       {
            ui->id_status->setStyleSheet("QLabel { color : green; }");
            ui->id_status->setText( tr( "Registered" ) );
            std::string public_key_string = public_key_address(_current_record->active_key);
            ui->public_key->setText( public_key_string.c_str() );
            if( _address_book != nullptr )
               save_contact->setEnabled(true);
       }
       else
       {
            ui->id_status->setStyleSheet("QLabel { color : red; }");
            ui->id_status->setText( tr( "Unable to find ID" ) );
            ui->public_key->setText(QString());
            save_contact->setEnabled(false);
       }
   } 
   catch ( const fc::exception& e )
   {
      ui->id_status->setText( e.to_string().c_str() );
   }
}

void ContactView::lookupPublicKey()
{
   std::string public_key_string = ui->public_key->text().toStdString();
   //fc::ecc::public_key public_key;
   //bts::bitname::client::reverse_name_lookup( public_key );
   bool public_key_is_registered = is_registered_public_key(public_key_string);
   if (public_key_is_registered)
     ui->id_edit->setText("********"); //any better idea to indicate unknown but keyhoteeId?
   else
     ui->id_edit->setText(QString()); //clear keyhotee field if unregistered public key
}

void  ContactView::setAddressBook( AddressBookModel* addressbook )
{
    _address_book = addressbook;
}
AddressBookModel* ContactView::getAddressBook()const
{
    return _address_book;
}


void ContactView::onPublicKeyToClipboard ()
{
   QClipboard *clip = QApplication::clipboard();
   clip->setText(ui->public_key->text());
   QMessageBox::information(this, tr("Clipboard"), 
                  tr("The Public Key has been copied to the clipboard."));
}

void ContactView::keyEdit (bool enable)
{      
   ui->firstname->setEnabled (enable);   
   ui->lastname->setEnabled (enable);
   ui->id_edit->setEnabled (enable);
   ui->public_key->setEnabled (enable);
   ui->privacy_comboBox->setEnabled (enable);
   ui->email->setEnabled (enable);
   ui->phone->setEnabled (enable);
   ui->notes->setEnabled (enable);

   ui->id_status->setVisible (enable);
   save_contact->setVisible (enable);
   cancel_edit_contact->setVisible (enable);
   send_mail->setVisible (! enable);
   edit_contact->setVisible (! enable);
   share_contact->setVisible (! enable);
   request_contact->setVisible (! enable);
   request_contact->setVisible (! enable);   

   ui->contact_pages->setTabEnabled (chat, ! enable);

   if (enable) ui->firstname->setFocus ();
}


void ContactView::onTabChanged(int index)
{
   if (index == chat) {
      //Question wykrywanie zmian
      keyEdit (false);
   }
}