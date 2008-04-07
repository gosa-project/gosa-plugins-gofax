<?php

$display  ="";
$lock_msg = "";

if (!$remove_lock){

	/* Reset requested? */
	if (isset($_POST['edit_cancel']) ||
		(isset($_GET['reset']) && $_GET['reset'] == 1)){

		del_lock ($ui->dn);
		session::un_set ('edit');
		session::un_set ('gofaxAccount');
	}

	/* Create gofaxAccount object on demand */
	if (!session::is_set('gofaxAccount') || (isset($_GET['reset']) && $_GET['reset'] == 1)){
		$gofaxAccount= new gofaxAccount ($config, $ui->dn);
		$gofaxAccount->set_acl_base($ui->dn);
		$gofaxAccount->set_acl_category("users");
		session::set('gofaxAccount',$gofaxAccount);
	}
	$gofaxAccount = session::get('gofaxAccount');

	/* save changes back to object */
	if (session::is_set('edit')){
		$gofaxAccount->save_object ();
	}

	/* Enter edit mode? */
	if (isset($_POST['edit'])){

		/* Check locking */
		if (($username= get_lock($ui->dn)) != ""){
			session::set('back_plugin',$plug);
			session::set('LOCK_VARS_TO_USE',array("/^edit$/","/^plug$/"));
			$lock_msg =  gen_locked_message ($username, $ui->dn);

		}else{

			/* Lock the current entry */
			add_lock ($ui->dn, $ui->dn);
			session::set('dn',$ui->dn);
			session::set('edit',TRUE);
		}
	}

	/* save changes to LDAP and disable edit mode */
	if (isset($_POST['edit_finish'])){

                /* Perform checks */
                $message= $gofaxAccount->check ();

                /* No errors, save object */
                if (count ($message) == 0){
                        $gofaxAccount->save ();
                        del_lock ($ui->dn);
                        session::un_set ('edit');

                } else {
                        /* Errors found, show message */
                        msg_dialog::displayChecks($message);
                }
        }

	/* Execute formular */
	if($lock_msg){
		$display.= $lock_msg;
	}else{
		$display.= $gofaxAccount->execute ();
	}

	$info= "";

	/* Store changes  in session */
	if (session::is_set('edit')){
		session::set('gofaxAccount',$gofaxAccount);
	}

	/* Show page footer depending on the mode */
	if (!$gofaxAccount->locals_dialog &&
		!$gofaxAccount->out_blocklist_dialog &&
		!$gofaxAccount->in_blocklist_dialog &&
		$gofaxAccount->is_account	&& 
		empty($lock_msg)){

		$display.= "<p class=\"plugbottom\">\n";

		/* Are we in edit mode? */
		if (session::is_set('edit')){
			$display.= "<input type=submit name=\"edit_finish\" style=\"width:80px\" value=\"".msgPool::okButton()."\">\n";
			$display.= "&nbsp;\n";
			$display.= "<input type=submit name=\"edit_cancel\" value=\"".msgPool::cancelButton()."\">\n";
			$info= "<img alt='' align=\"middle\" src=\"".get_template_path('images/closedlock.png')."\"> ".$ui->dn."&nbsp;";
		} else {
			$info= "<img alt=\"\" align=\"middle\" src=\"".get_template_path('images/openlock.png')."\"> ".$ui->dn."&nbsp;";

			if(preg_match("/w/",$ui->get_permissions($ui->dn,"users/gofaxAccount"))){
				$info.= "<img alt=\"\" align=\"middle\" src=\"".get_template_path('images/lamp.png')."\">"._("Click the 'Edit' button below to change informations in this dialog");
				$display.= "<input type=submit name=\"edit\" value=\"".msgPool::editButton()."\">";
			}
			$display.= "<input type=\"hidden\" name=\"ignore\">\n";
		}
		$display.= "</p>";
	}
	
	/* Page header*/
	$display= print_header(get_template_path('images/fax.png'), _("FAX settings"), $info).$display;

}
?>