<?php

class gofaxAccount extends plugin
{
  /* Definitions */
  var $plHeadline= "FAX";
  var $plDescription= "Manage personal facsimile settings";
  var $plIcon = "plugins/gofax/images/plugin.png";

  var $departments;

  /* Fax attributes */
  var $goFaxDeliveryMode= "";
  var $facsimileTelephoneNumber= "";
  var $goFaxIsEnabled= 1;
  var $goFaxPrinter= "";
  var $goFaxDivertNumber= "";
  var $goFaxLanguage= "";
  var $goFaxFormat= "";
  var $goFaxRBlocklist = array();
  var $goFaxRBlockgroups= array();
  var $goFaxSBlocklist= array();
  var $goFaxSBlockgroups= array();
  var $mail= "";
  var $facsimileAlternateTelephoneNumber= array();
  var $fax_formats = array("pdf","ps","png","mtiff","tiff");  

  /* Internal variables */
  var $printerList= array();
  var $has_mailAccount= FALSE;
  var $faxNumberSelect= FALSE;
  var $in_blocklist_dialog= FALSE;
  var $out_blocklist_dialog= FALSE;
  var $current_blocklist= array();
  var $view_logged = FALSE;

  /* Copy & paste variables */
  var $CopyPasteVars=array("facsimileTelephoneNumber");//,"goFaxRBlocklist","goFaxRBlockgroups","goFaxSBlocklist","goFaxSBlockgroups");

  /* attribute list for save action */
  var $attributes= array("goFaxDeliveryMode", "goFaxIsEnabled","facsimileAlternateTelephoneNumber","goFaxRBlocklist","goFaxRBlockgroups","goFaxSBlocklist","goFaxSBlockgroups","goFaxPrinter", "goFaxDivertNumber", "goFaxLanguage", "goFaxFormat", "mail","facsimileTelephoneNumber");

  var $uid ="";

  var $objectclasses= array("goFaxAccount");

  /* Enable multiple edit support */
  var $multiple_support = TRUE;

  function __construct(&$config, $dn= NULL)
  {
    /* General initialization */
    plugin::__construct ($config, $dn);

    /* Set uid, it is used in handle_post_events */
    if(isset($this->attrs['uid'])){
      $this->uid = $this->attrs['uid'][0];
    }

    /* Hickert : 11.11.05
     */
    if(!session::is_set("Fax_Filter")){
      $Fax_Filter = array();
      $Fax_Filter['depselect'] = $this->config->current['BASE'];
      session::set('Fax_Filter',$Fax_Filter);
    }

    if ($dn != "new"){
      /* Get arrays */
      foreach (array("goFaxRBlocklist", "goFaxRBlockgroups", "goFaxSBlocklist",
            "goFaxSBlockgroups", "facsimileAlternateTelephoneNumber") as $val){
        $this->$val =array();
        if (isset($this->attrs["$val"]["count"])){
          for ($i= 0; $i<$this->attrs["$val"]["count"]; $i++){
            array_push($this->$val, $this->attrs["$val"][$i]);
          }
        }
      }

      /* Set up has_mailAccount */
      if (in_array_strict("gosaMailAccount", $this->attrs['objectClass'])){
        $this->has_mailAccount= TRUE;
      }
    }

    /* Load printer list */
    $this->printerList=get_printer_list();

    /* Check if the currently selected printer is still available. 
       If not, append current printer to list of available.
       It could be possible, that we are not allowed to view printers and so the list is empty ...  */
    if(!empty($this->goFaxPrinter) && !isset($this->printerList[$this->goFaxPrinter])) {
      $this->printerList[$this->goFaxPrinter] = "[".$this->goFaxPrinter."]";
    }

    // Prepare lists
    $this->predefinedList = new sortableListing();
    $this->predefinedList->setDeleteable(false);
    $this->predefinedList->setEditable(false);
    $this->predefinedList->setWidth("100%");
    $this->predefinedList->setHeight("300px");
    $this->predefinedList->setColspecs(array('20px','*'));
    $this->predefinedList->setHeader(array("~",_("Name")));
    $this->predefinedList->setDefaultSortColumn(1);
  }

  function execute()
  {
    /* Call parent execute */
    plugin::execute();

    /* Log view */
    if($this->is_account && !$this->view_logged){
      $this->view_logged = TRUE;
      new log("view","users/".get_class($this),$this->dn);
    }

    /* Department selection handling
     */
    if((isset($_GET['act']))&&($_GET['act']=="dep_open")){
      $Fax_Filter = session::get('Fax_Filter');
      if(strlen(base64_decode($_GET['dep_id']))>= strlen($this->config->current['BASE'])){
        $Fax_Filter['depselect']= base64_decode($_GET['dep_id']);
      }else{
        $Fax_Filter['depselect']= $this->config->current['BASE'];
      }
      session::set('Fax_Filter',$Fax_Filter);
    }

    /* Edit mode specifies if we are editing from my accout */
    $edit_mode = (!is_object($this->parent) && !session::is_set('edit'));

    /* Load smarty stuff */
    $smarty= get_smarty();

    /* Check if mail account is active. We'll display an additional
       mail address field if this is not the case. Some people may
       want goFax, but have a mailserver which is not managed with
       GOsa */
    if (!@isset($this->parent->by_object['mailAccount'])) {
      $smarty->assign("has_mailaccount", $this->has_mailAccount?"true":"false");
    } elseif ( !$this->parent->by_object['mailAccount']->is_account){
      $smarty->assign("has_mailaccount", "false");
      $this->has_mailAccount= false;
    } else {
      $smarty->assign("has_mailaccount", "true");
    }


    $display = "";
    if(!$this->multiple_support_active){    

      /* Do we need to flip is_account state? */
      if (isset($_POST['modify_state'])){
        $this->is_account= !$this->is_account;
      }

      /* Do we represent a valid account? */
      if (!$this->is_account && $this->parent === NULL){
        $display= "<img alt=\"\" src=\"images/small-error.png\" align=\"middle\">&nbsp;<b>".
          msgPool::noValidExtension(_("GOfax"))."</b>";
        $display.= back_to_main();
        return ($display);
      }

      /* Show tab dialog headers */
      $display= "";
      if ($this->parent !== NULL){
        if ($this->is_account){
          $display= $this->show_disable_header(_("Remove FAX account"),
              msgPool::featuresEnabled(_("GOfax")));
        } else {
          $display= $this->show_enable_header(_("Create FAX account"),
              msgPool::featuresDisabled(_("GOfax")));
          return ($display);
        }
      }
    }

    /* Trigger Add local fax alternatives dialog */
    if (isset($_POST['add_local_alternate'])){
      if($this->acl_is_writeable("facsimileAlternateTelephoneNumber",$edit_mode)){
        $this->faxNumberSelect = new faxNumberSelect($this->config, get_userinfo());
        $this->dialog= TRUE;	
      }
    }

    if (isset($_POST['faxNumberSelect_cancel']) && $this->faxNumberSelect instanceOf faxNumberSelect){
      $this->dialog = $this->faxNumberSelect = FALSE;
    }

    /* Add alternatives from dialog */
    if (isset($_POST['faxNumberSelect_save']) && $this->faxNumberSelect instanceOf faxNumberSelect){
      if($this->acl_is_writeable("facsimileAlternateTelephoneNumber",$edit_mode)){

        $entries = $this->faxNumberSelect->save();
        foreach ($entries as $entry){
          $this->addAlternate($entry['facsimileTelephoneNumber'][0]);
          $this->is_modified= TRUE;
        }
      }
      $this->dialog = $this->faxNumberSelect = FALSE;
    }

    if ($this->faxNumberSelect){

      // Build up blocklist
      $list = $this->facsimileAlternateTelephoneNumber;
      $list[] = $this->facsimileTelephoneNumber;
      session::set('filterBlacklist', array('facsimileTelephoneNumber' => $list));
      return($this->faxNumberSelect->execute());
    }

    /* Add alternatives */
    if (isset($_POST['add_alternate']) && !empty($_POST['forward_address']) && tests::is_phone_nr($_POST['forward_address'])){ 
      if($this->acl_is_writeable("facsimileAlternateTelephoneNumber",$edit_mode)){
        $this->addAlternate($_POST['forward_address']);
      }
    }

    /* Delete alternate fax number */
    if (isset($_POST['delete_alternate']) && isset($_POST['alternate_list']) && count($_POST['alternate_list'])){ 
      if($this->acl_is_writeable("facsimileAlternateTelephoneNumber",$edit_mode)){
        $this->delAlternate ($_POST['alternate_list']);
      }
    }


    /* Edit incoming blocklists */
    if (isset($_POST['edit_incoming'])){
      if($this->acl_is_writeable("goFaxRBlocklist",$edit_mode))  {
        $this->current_blocklist= array_merge($this->goFaxRBlocklist,$this->goFaxRBlockgroups);
        sort($this->current_blocklist);
        reset($this->current_blocklist);

        $this->in_blocklist_dialog= TRUE;
        $this->dialog= TRUE;
      }
    }

    /* Edit outgoing blocklists */
    if (isset($_POST['edit_outgoing'])){
      $this->current_blocklist= array_merge($this->goFaxSBlocklist,$this->goFaxSBlockgroups);
      sort($this->current_blocklist);
      reset($this->current_blocklist);

      $this->out_blocklist_dialog= TRUE;
      $this->dialog= TRUE;
    }

    /* Add number to blocklist (dialog) */
    if (isset($_POST['add_blocklist_number']) && $_POST['block_number'] != ""){
      if (!tests::is_phone_nr($_POST['block_number'])){
        msg_dialog::display(_("Error"), msgPool::invalid(_("Phone number")), ERROR_DIALOG);
      } else {
        array_push($this->current_blocklist, $_POST['block_number']);
        $this->current_blocklist= array_unique($this->current_blocklist);
        sort($this->current_blocklist);
        reset($this->current_blocklist);
      }
    }

    /* Add list to blocklist */
    if (isset($_POST['add_blocklist'])){
      foreach($_POST as $name => $value){
        if(preg_match("/ADDLIST_/i",$name)){
          $this->current_blocklist= array_merge($this->current_blocklist, array(base64_decode($value)));

        }
      } 
      $this->current_blocklist= array_unique($this->current_blocklist);
      sort($this->current_blocklist);
      reset($this->current_blocklist);
    }

    /* Add list to blocklist */
    if (isset($_GET['add'])){
      if(!is_array($this->current_blocklist)) $this->current_blocklist=array();
      $this->current_blocklist= array_merge($this->current_blocklist, array( base64_decode($_GET['add'])));
      $this->current_blocklist= array_unique($this->current_blocklist);
      sort($this->current_blocklist);
      reset($this->current_blocklist);
    }

    /* Delete from blocklist */
    if (isset($_POST['delete_blocklist_number']) && isset($_POST['block_list'])){
      $tmp= array();
      foreach($this->current_blocklist as $val){
        if (!in_array_strict($val, $_POST['block_list'])){
          $tmp[]= $val;
        }
      }
      $this->current_blocklist= $tmp;
    }


    /* Blocklist edit finished */
    if (isset($_POST['edit_blocklists_finish'])){

      /* Incoming or outgoing? */
      if ($this->in_blocklist_dialog){
        $this->goFaxRBlocklist = array();
        $this->goFaxRBlockgroups = array();

        foreach ($this->current_blocklist as $val){
          if (tests::is_phone_nr($val)){
            $this->goFaxRBlocklist[]=$val;
          } else {
            $this->goFaxRBlockgroups[]= $val;
          }
        }
      } else {
        $this->goFaxSBlocklist = array();
        $this->goFaxSBlockgroups = array();

        /* Transfer values to ourself */
        foreach ($this->current_blocklist as $val){
          if (tests::is_phone_nr($val)){
            $this->goFaxSBlocklist[]=$val;
          } else {
            $this->goFaxSBlockgroups[]= $val;
          }
        }
      }
      $this->is_modified= TRUE;
    }


    /* Set departments */
    if ( $this->in_blocklist_dialog || $this->out_blocklist_dialog){
      
      $list= array ();
      $ldap= $this->config->get_ldap_link();
      if (isset ($_POST['department'])){
        $ldap->cd ($_POST['department']);
      } else {
        $ldap->cd ($this->config->current['BASE']);
      }
    }

    /* Cancel  dialogs */
    if (isset($_POST['edit_blocklists_finish']) || isset($_POST['edit_blocklists_cancel'])){
      $this->in_blocklist_dialog= FALSE;
      $this->out_blocklist_dialog= FALSE;
      $this->dialog= FALSE;
    }

    /* Manage locals dialog */
    if ($this->faxNumberSelect){
      return($this->faxNumberSelect->execute());
    }


    /* Manage incoming blocklists */
    if ($this->in_blocklist_dialog){

      /* The base specifies the current position in the ldap tree 
       * The current base was specified by $_GET['dep_id'] before. Or contains the default value.
       */
      $Fax_Filter = session::get('Fax_Filter');
      $base = $Fax_Filter['depselect'];
      $ldap->cd($base);

      /* Ge all Blocklists */
      $ldap->search ("(objectClass=goFaxRBlock)",array("cn","description"));
      while ($attrs= $ldap->fetch()){

        /* Generate list depending on description */
        if(isset($attrs['description'][0])){
          $list[$attrs['cn'][0]]=
            $attrs['description'][0].
            " [".$attrs['cn'][0]."]";
        }else{
          $list[$attrs['cn'][0]]= $attrs['cn'][0];
        }
      }

      /* NEW LIST MANAGMENT
       * We also need to search for the departments
       * So we are able to navigate like in konquerer
       */
      $this->departments= array();
   
      /* Get all departments within the current department */ 
      $Fax_Filter = session::get('Fax_Filter');
      $base = $Fax_Filter['depselect'];
      $ldap->ls("(objectClass=gosaDepartment)",$base);

      /* Base back is used to go one department up in our ldap tree */
      $base_back = preg_replace("/^[^,]+,/","",$base);

      /* Only show base_back if it is needed */
      if((strlen($base_back)>= strlen($this->config->current['BASE']))&&($base!=$this->config->current['BASE'])){
        $this->departments[preg_replace("/^[^,]+,/","",$base)] = ".. - ["._("back")."]"; 
      }

      /* Fetch all returned departments an add them to our listing */
      while($value = $ldap->fetch()){
        if($value["description"][0]!=".."){
          $this->departments[$value['dn']]=LDAP::fix(convert_department_dn($value['dn'])." - [".$value["description"][0]."]");
        }else{
          $this->departments[$value['dn']]=$value["description"][0];
        }
      }
      /* END NEW LIST MANAGMENT
       */

      // This links specifies the department open link 
      $linkopen = "<a href='?plug=".$_GET['plug']."&amp;act=dep_open&amp;dep_id=%s'>%s</a>";

      /* Insert departments in divsel */
        $data = $lData = array();
      foreach($this->departments as $key=> $val){
        if(!isset($this->config->departments[trim($key)])){
          $this->config->departments[trim($key)]="";
        }
        $img = image('images/lists/folder.png');
        $val = sprintf( $linkopen, postEncode($key), $val);
        $data[] = array('TYPE' => 'DEP', 'ID' => $key);
        $lData[] = array('data' => array($img,$val));
      }


      /* Append predefined Blocklists */
      foreach($list as $entry => $value){
          $chk = "<input type='checkbox' value='".base64_encode($entry)."' 
              name='ADDLIST_".base64_encode($entry)."'>";
          $data[] = array('TYPE' => 'ENTRY', 'ID' => $key);
          $lData[] = array('data' => array($chk,$value));
      }

      $this->predefinedList->setAcl('rwcdm');
      $this->predefinedList->setListData($data,$lData);
      $this->predefinedList->update();


      /* Show dialog */
      $smarty->assign("cblocklist", set_post($this->current_blocklist));
      $smarty->assign("predefinedList", $this->predefinedList->render());
      $display.= $smarty->fetch (get_template_path('lists.tpl', TRUE, dirname(__FILE__)));
      return ($display);
    }

    /* Manage outgoing blocklists */

    if ($this->out_blocklist_dialog){

      /* The base specifies the current position in the ldap tree
       * The current base was specified by $_GET['dep_id'] before. Or contains the default value.
       */
      $Fax_Filter = session::get('Fax_Filter');
      $base = $Fax_Filter['depselect'];
      $ldap->cd($base);

      /* Ge all Blocklists */ 
      $ldap->search ("(objectClass=goFaxSBlock)",array("cn","description"));
      while ($attrs= $ldap->fetch()){
        if(isset($attrs['description'][0])){
          $list[$attrs['cn'][0]]=
            $attrs['description'][0].
            " [".$attrs['cn'][0]."]";
        }else{
          $list[$attrs['cn'][0]]= $attrs['cn'][0];
        }
      }

      /* NEW LIST MANAGMENT
       * We also need to search for the departments
       * So we are able to navigate like in konquerer
       */
      $this->departments= array();
      $ldap->ls("(objectClass=gosaDepartment)",$base);

      /* Generate Back url, and append if it is needed */
      $base_back = preg_replace("/^[^,]+,/","",$base);
      if((strlen($base_back)>= strlen($this->config->current['BASE']))&&($base!=$this->config->current['BASE'])){
        $this->departments[preg_replace("/^[^,]+,/","",$base)] = ".. - ["._("back")."]";
      }

      /* Get all departments */
      while($value = $ldap->fetch()){
        if(isset($value["description"][0])){
          $this->departments[$value['dn']]=LDAP::fix(convert_department_dn($value['dn'])." - [".$value["description"][0]."]");
        }else{
          $this->departments[$value['dn']]=$value["description"][0];
        }
      }
      /* END NEW LIST MANAGMENT
       */

      /* Insert departments in divsel */
      $linkopen = "<a href='?plug=".$_GET['plug']."&amp;act=dep_open&amp;dep_id=%s'>%s</a>";
      $data = $lData = array();
      foreach($this->departments as $key=> $val){
        if(!isset($this->config->departments[trim($key)])){
          $this->config->departments[trim($key)]="";
        }
        $img = image('images/lists/folder.png');
        $val = sprintf( $linkopen, postEncode($key), $val);
        $data[] = array('TYPE' => 'DEP', 'ID' => $key);
        $lData[] = array('data' => array($img,$val));
      }


      /* Append predefined Blocklists */
      foreach($list as $entry => $value){
          $chk = "<input type='checkbox' value='".base64_encode($entry)."' 
              name='ADDLIST_".base64_encode($entry)."'>";
          $data[] = array('TYPE' => 'ENTRY', 'ID' => $key);
          $lData[] = array('data' => array($chk,$value));
      }

      $this->predefinedList->setAcl('rwcdm');
      $this->predefinedList->setListData($data,$lData);
      $this->predefinedList->update();

      /* Show dialog */
      $smarty->assign("cblocklist", set_post($this->current_blocklist));
      $smarty->assign("predefinedList", $this->predefinedList->render());
      $display.= $smarty->fetch (get_template_path('lists.tpl', TRUE, dirname(__FILE__)));
      return ($display);
    }


    /* Show main page */
    $smarty->assign("languages", get_languages(TRUE));
    $smarty->assign("formats",  set_post($this->fax_formats));
    $smarty->assign("printers", set_post($this->printerList));

    /* Load attributes */
    foreach(array("goFaxIsEnabled", "goFaxDeliveryMode", "facsimileTelephoneNumber",
          "goFaxPrinter", "goFaxLanguage", "goFaxFormat", 
          "facsimileAlternateTelephoneNumber", "mail") as $val){

      $smarty->assign("$val", set_post($this->$val));
    }

    $tmp = $this->plInfo();
    foreach($tmp['plProvidedAcls'] as $acl => $desc){
      $smarty->assign($acl."ACL",$this->getacl($acl,$edit_mode));
    }

    /* Load checkboxes */
    if ($this->goFaxIsEnabled == "1"){
      $smarty->assign("goFaxIsEnabled", "");
    } else {
      $smarty->assign("goFaxIsEnabled", "checked");
    }
    /* goFaxAccount has "mail" as must! Block if no mailaddress is specified... */
    if ($this->goFaxDeliveryMode & 32) {
      $smarty->assign("faxtomail", "checked");
    } else {
      $smarty->assign("faxtomail", "");
    }
    if ($this->goFaxDeliveryMode & 64) {
      $smarty->assign("faxtoprinter", "checked");
    } else {
      $smarty->assign("faxtoprinter", "");
    }

    foreach($this->attributes as $attr){
      if(in_array_strict($attr,$this->multi_boxes)) {
        $smarty->assign("use_".$attr,TRUE);
      }else{
        $smarty->assign("use_".$attr,FALSE);
      }
    }
    foreach(array("goFaxIsEnabled","edit_outgoing","edit_incoming","faxtomail","faxtoprinter") as $attr){
      if(in_array_strict($attr,$this->multi_boxes)) {
        $smarty->assign("use_".$attr,TRUE);
      }else{
        $smarty->assign("use_".$attr,FALSE);
      }
    }
  
    $smarty->assign("multiple_support",$this->multiple_support_active);
    $display.= $smarty->fetch (get_template_path('generic.tpl', TRUE, dirname(__FILE__)));
    return ($display);
  }

  function remove_from_parent()
  {
    /* Cancel if there's nothing to do here */
    if (!$this->initially_was_account){
      return;
    }

    plugin::remove_from_parent();

    /* Adapt mail settings if needed */
    if ((isset($this->parent->by_object['mailAccount']->is_account)) && ($this->parent->by_object['mailAccount']->is_account)){
      unset($this->attrs['mail']);
    }

    $ldap= $this->config->get_ldap_link();
    $ldap->cd($this->dn);
    $this->cleanup();
    $ldap->modify ($this->attrs); 

    new log("remove","users/".get_class($this),$this->dn,array_keys($this->attrs),$ldap->get_error());

    if (!$ldap->success()){
      msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, 0, __CLASS__));
    }

    /* Optionally execute a command after we're done */
    $this->handle_post_events('remove',array("uid"=> $this->uid));
  }


  /* Check formular input */
  function check()
  {
    /* Call common method to give check the hook */
    $message= plugin::check();

    /* must: facsimileTelephoneNumber */
    if ($this->facsimileTelephoneNumber == ""){
      $message[]= msgPool::required(_("Fax number"));
    }

    if (!tests::is_phone_nr($this->facsimileTelephoneNumber)){
      $message[]= msgPool::invalid(_("Fax number"),$this->facsimileTelephoneNumber,"/[0-9]/");
    }

    // check for a valid Mail-Address in case of 'delivery to mailbox'
    $mailAccount = (isset($this->parent->by_object['mailAccount']) && $this->parent->by_object['mailAccount']->is_account);
    if(!$mailAccount && $this->goFaxDeliveryMode & 32){
      if ($this->mail == ""){
        $message[]= _("Mail delivery is requested without target address!");
      } elseif (!tests::is_email($this->mail)){
        $message[]= msgPool::invalid(_("Mail address"),"","",_("your-name@your-domain.com"));
      }
    }

    // IE Fix, IE lets you choose disabled option, stupid browser ... 
    if((empty($this->goFaxPrinter))&&($this->goFaxDeliveryMode & 64)){
      $message[]= _("Printing is requested without a target printer!");
    }

    return ($message);
  }

  /* Save data to object */
  function save_object()
  {
    $edit_mode = (!is_object($this->parent) && !session::is_set('edit'));
    if (isset($_POST['faxTab'])){
      plugin::save_object();


      $tmp = 0+$this->goFaxDeliveryMode;

      if($this->acl_is_writeable("faxtomail",$edit_mode)){
        if (isset($_POST["faxtomail"]) && $_POST["faxtomail"] == 1){  
          $tmp |= 32;
        }elseif($tmp & 32){
          $tmp &= (!32);
        }
      }
      if($this->acl_is_writeable("faxtoprinter",$edit_mode)){
        if (isset($_POST["faxtoprinter"]) && $_POST["faxtoprinter"] == 1){  
          $tmp |= 64;
        }elseif($tmp & 64){
          $tmp &= !64;
        }
        if(isset($_POST['goFaxPrinterSelected'])){
          $this->goFaxPrinter =get_post('goFaxPrinterSelected');
        }
      }
      $this->goFaxDeliveryMode = $tmp;

      if($this->acl_is_writeable("goFaxIsEnabled",$edit_mode)){
        if (isset($_POST["goFaxIsEnabled"]) && $_POST["goFaxIsEnabled"] == "1"){
          $this->goFaxIsEnabled= "0";
        } else {
          $this->goFaxIsEnabled= "1";
        }
      }

    
      if (isset($_POST['mail']) && $this->acl_is_writeable("faxtomail",$edit_mode)){
        $this->mail= get_post('mail');
      }

      /* Check if mail account is active and correct the internal
         reference to represent the current status. */
      if(isset($this->parent)){
        if (isset($this->parent->by_object['mailAccount']->is_account)&&($this->parent->by_object['mailAccount']->is_account)){
          $this->has_mailAccount= TRUE;
        }
      }
    }

  }


  /* Save to LDAP */
  function save()
  {
    plugin::save();

    /* Save arrays */
    foreach (array("goFaxRBlocklist", "goFaxRBlockgroups", "goFaxSBlocklist",
          "goFaxSBlockgroups", "facsimileAlternateTelephoneNumber") as $val){

      $this->attrs[$val]= $this->$val;
    }

    if(!$this->attrs['goFaxDeliveryMode']){
      $this->attrs['goFaxDeliveryMode'] = 0;
    }
  
    /* Do not save mail address ... it was possibly changed by mail plugin */
    /* Adapt mail settings if needed */
    if ((isset($this->parent->by_object['mailAccount']->is_account)) && ($this->parent->by_object['mailAccount']->is_account)){
      unset($this->attrs['mail']);
    }

    /* Write back to ldap */
    $ldap= $this->config->get_ldap_link();
    $ldap->cd($this->dn);
    $this->cleanup();
    $ldap->modify ($this->attrs); 

    /* Log last action */
    if($this->initially_was_account){
      new log("modify","users/".get_class($this),$this->dn,array_keys($this->attrs),$ldap->get_error());
    }else{
      new log("create","users/".get_class($this),$this->dn,array_keys($this->attrs),$ldap->get_error());
    }

    if (!$ldap->success()){
      msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, 0, __CLASS__));
    }

    /* Optionally execute a command after we're done */
    if ($this->initially_was_account == $this->is_account){
      if ($this->is_modified){
        $this->handle_post_events("modify",array("uid" => $this->uid));
      }
    } else {
      $this->handle_post_events("add",array("uid" => $this->uid));
    }

  }


  /* Adapt from template, using 'dn' */
  function adapt_from_template($dn, $skip= array())
  {
    plugin::adapt_from_template($dn, $skip);

    foreach (array("goFaxRBlocklist", "goFaxRBlockgroups", "goFaxSBlocklist",
          "goFaxSBlockgroups", "facsimileAlternateTelephoneNumber") as $val){

      if (in_array_strict($val, $skip)){
        continue;
      }

      if (isset($this->attrs[$val])){
        $this->$val= $this->attrs[$val];
      }
    }
  }



  /* Add alternate fax recipient */
  function addAlternate($number)
  {
    $this->facsimileAlternateTelephoneNumber[]= "$number";
    $this->facsimileAlternateTelephoneNumber=
      array_unique ($this->facsimileAlternateTelephoneNumber);

    sort ($this->facsimileAlternateTelephoneNumber);
    reset ($this->facsimileAlternateTelephoneNumber);
  }

  function delAlternate($numbers)
  {
    $this->facsimileAlternateTelephoneNumber= array_remove_entries ($numbers,
        $this->facsimileAlternateTelephoneNumber);
  }

  function getCopyDialog()
  {
    $str    = "";
    $smarty = get_smarty();
    $smarty->assign("facsimileTelephoneNumber", set_post($this->facsimileTelephoneNumber));
    $str['string']   = $smarty->fetch(get_template_path("paste_generic.tpl",TRUE,dirname(__FILE__)));
    $str['status'] ="";
    return($str);
  }

  function SaveCopyDialog()
  {
    if(isset($_POST['facsimileTelephoneNumber'])){
      $this->facsimileTelephoneNumber = get_post('facsimileTelephoneNumber');
    }
  }


  /* Return plugin informations for acl handling 
      #FIXME some attributes are still missing in this plugin acls */
  static function plInfo()
  {
    return (array(  
          "plShortName"     => _("Fax"),
          "plDescription"   => _("Fax account settings"),
          "plSelfModify"    => TRUE,
          "plDepends"       => array("user"),
          "plPriority"      => 6,             
          "plSection"       => "personal",  
          "plCategory"      => array("users"),
          "plRequirements"=> array(
              'ldapSchema' => array('goFaxAccount' => '>=1.0.4'),
              'onFailureDisablePlugin' => array(__CLASS__,'gofaxAccount','goFaxServer')
              ),
          "plOptions"       => array(),

          "plProvidedAcls"  => array(
            "facsimileTelephoneNumber"          => _("Fax number"), // goFaxDeliveryMode
            "goFaxLanguage"                     => _("Language"),
            "goFaxFormat"                       => _("Delivery format"),
            "facsimileAlternateTelephoneNumber" => _("Alternate FAX number"), // goFaxDeliveryMode
            "goFaxIsEnabled"                    => _("Enable/Disable FAX"),
            "faxtomail"                         => _("Deliver FAX as mail"),
            "faxtoprinter"                      => _("Deliver FAX to printer"),
            "goFaxRBlocklist"                   => _("Receive blacklist"),
            "goFaxSBlocklist"                   => _("Send blacklist"))
          ));
  }

  /* Multiple edit support 
   */
  
  /* Execute */
  function multiple_execute()
  {
    return($this->execute());
  }

  function multiple_save_object()
  {
    if (isset($_POST['faxTab'])){
      plugin::multiple_save_object();
      foreach(array("goFaxIsEnabled","edit_outgoing","edit_incoming","faxtomail","faxtoprinter") as $attr){
        if(isset($_POST["use_".$attr])){
          $this->multi_boxes[] = $attr;
        }
      }

      $tmp = 0+$this->goFaxDeliveryMode;
      if(in_array_strict("faxtomail",$this->multi_boxes)){
        if (isset($_POST["faxtomail"]) && $_POST["faxtomail"] == 1){
          $tmp |= 32;
        }elseif($tmp & 32){
          $tmp &= (!32);
        }
      }
      if(in_array_strict("faxtoprinter",$this->multi_boxes)){
        if (isset($_POST["faxtoprinter"]) && $_POST["faxtoprinter"] == 1){
          $tmp |= 64;
        }elseif($tmp & 64){
          $tmp &= !64;
        }
      }
      $this->goFaxDeliveryMode = $tmp;

        if (isset($_POST["goFaxIsEnabled"]) && $_POST["goFaxIsEnabled"] == "1"){
          $this->goFaxIsEnabled= "0";
        } else {
          $this->goFaxIsEnabled= "1";
        }
      if (isset($_POST['mail']) && in_array_strict("mail",$this->multi_boxes)){
        $this->mail= get_post('mail');
      }
    }
  }

  function get_multi_edit_values()
  {
    $ret = plugin::get_multi_edit_values();
    if(in_array_strict("goFaxIsEnabled",$this->multi_boxes)){
      $ret['goFaxIsEnabled'] = $this->goFaxIsEnabled;
    }
    if(in_array_strict("faxtoprinter",$this->multi_boxes)){
      $ret['faxtoprinter'] = $this->goFaxDeliveryMode & 64;
    }
    if(in_array_strict("faxtomail",$this->multi_boxes)){
      $ret['faxtomail'] = $this->goFaxDeliveryMode & 32;
    }

    if(in_array_strict("edit_incoming",$this->multi_boxes)){
      $ret['goFaxRBlocklist'] = $this->goFaxRBlocklist;
      $ret['goFaxRBlockgroups'] = $this->goFaxRBlockgroups;
    }
    if(in_array_strict("edit_outgoing",$this->multi_boxes)){
      $ret['goFaxSBlocklist'] = $this->goFaxSBlocklist;
      $ret['goFaxSBlockgroups'] = $this->goFaxSBlockgroups;
    }
    return($ret);
  }

  function multiple_check()
  {
    $message = plugin::multiple_check();
    return($message);
  }

  function set_multi_edit_values($values)
  {
    plugin::set_multi_edit_values($values);

    if(isset($values['faxtoprinter'])){
      if($values['faxtoprinter']){
        $this->goFaxDeliveryMode |= 64;
      }else{
        $this->goFaxDeliveryMode &= !64;
      }
    }
    if(isset($values['faxtomail'])){
      if($values['faxtomail']){
        $this->goFaxDeliveryMode |= 32;
      }else{
        $this->goFaxDeliveryMode &= !32;
      }
    }
  }
}


?>
