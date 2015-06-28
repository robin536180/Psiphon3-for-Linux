/*////////////////////////////////////////////////////////////////////////
Copyright (c) 2002-2006 National Institute of Advanced Industrial Science and Technology (AIST)
        
Permission to use this material for noncommercial and/or evaluation
purpose, copy this material for your own use, and distribute the copies
via publicly accessible on-line media, without fee, is hereby granted
provided that the above copyright notice and this permission notice
appear in all copies.
AIST MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY OF THIS
MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", WITHOUT ANY EXPRESS
OR IMPLIED WARRANTIES.
/////////////////////////////////////////////////////////////////////////
Content-Type:	program/C; charset=US-ASCII
Program:	htswitch.c (HTTP URL access control)
Author:		Yutaka Sato <ysato@delegate.org>
Description:
  purpose
    - force to see an access control page, and
    - restrict access control only by the control page

    - the same random key both in Cookie and Request (GET-URL or POST body)
    - random key generated by some secret in DeleGate thus difficult to forge

    the switch controls acception of request, to accpet only if the same
    "action=serverid.randpass" is both in Cookie and POST body

	POST /url?action HTTP/1.0
	Cookie:Key-action=serverid.randpass
	<CRLF>
	action=serverid.randpass

    the necessary cookie "Key-action=serverid.randpass" is generated by
    DeleGate to be set and reset, reacting to the following requests.

	POST /url?action HTTP/1.0
	Cookie:Key-control=serverid.randpass
	<CRLF>
	ON-action=serverid.randpass

	-> HTTP/1.0 200
           Set-Cookie: Key-action=serverid.randpass

	POST /url?action HTTP/1.0
	Cookie:Key-control=serverid.randpass
	<CRLF>
	OFF-action=serverid.randpass

	-> HTTP/1.0 200
           Set-Cookie: Key-action=off

    the request to enable an action, for example, is generated and sent
    from a client clicking a button for FORM like this.

	<FORM ACTION=/path?action METHOD=POST>
	<INPUT TYPE=hidden NAME=ON-action VALUE=serverid.randpass>
	<INPUT TYPE=submit VALUE=ON-action>
	</FORM>

     the necessary cookie "Key-control:serverid.randpass" is generated by
     DeleGate on user's access to the "control page".  Thus a user is
     forced to see "control page" to control the accecibility.
     This will prevent
       - access via hidden hyper-link to gurded URLs
       - access from robots to gurded URLs

History:
	020914	extracted from nntpgw.c and generalized
//////////////////////////////////////////////////////////////////////#*/
#include <stdio.h>
#include <ctype.h>
#include "delegate.h"
#include "ystring.h"
#include "htswitch.h"

void HTSWputControlForm(FILE *tc,HtSwitch *sw,PCStr(urlself),PCStr(nkey))
{	CStr(maxage,32);

	fprintf(tc,"%s is ",sw->s_swdesc);
	fprintf(tc,"<B>%s</B>.\r\n",sw->s_beon ? "enabled":"disabled");
	fprintf(tc,"<BIG><MENU>\n");

	if( sw->s_beon ){
	fprintf(tc,"<FORM ACTION=%s?%s METHOD=POST>",urlself,sw->s_requrl);
	fprintf(tc,"<INPUT TYPE=hidden NAME=%s VALUE=%s>",sw->s_reqcmd,nkey);
	fprintf(tc,"<INPUT TYPE=submit VALUE=%s>",sw->s_blabel);

	if( sw->s_bcmmnt )
	fprintf(tc,"%s\n",sw->s_bcmmnt);

	fprintf(tc,"</FORM>\n");
	}

	fprintf(tc,"<FORM ACTION=%s?%s METHOD=POST>",urlself,sw->s_ctlurl);
	fprintf(tc,"<INPUT TYPE=hidden NAME=%s%s VALUE=%s>",
		sw->s_beon?"OFF-":"ON-",sw->s_reqcmd,nkey);
	fprintf(tc,"<INPUT TYPE=submit VALUE=%s>",
		sw->s_beon?"disable":"enable");
	fprintf(tc,"</FORM>\n");
	fprintf(tc,"</MENU></BIG>\n");
}
void HTSWputControlCookie(FILE *tc,HtSwitch *sw,PCStr(path),PCStr(nkey))
{	const char *key;
	CStr(setcookie,256);

	if( sw->s_doset == 0 )
		return;

	if( sw->s_beon )
		key = nkey;
	else	key = "off";

	sprintf(setcookie,"Key-%s=%s; Path=%s",sw->s_reqcmd,key,path);
	if( sw->s_maxage && sw->s_beon )
		Xsprintf(TVStr(setcookie),"; Max-Age=%d",
			sw->s_maxage);

	sv1log("Set-Cookie: %s\n",setcookie);
	fprintf(tc,"Set-Cookie: %s\r\n",setcookie);
}
extern int START_TIME;
int HTSWmyid(){
	int myid;
	myid = START_TIME % 10000;
	return myid;
}
void HTSWnewkey(PVStr(nkey),int myid)
{	double rtime;
	int itime;

	rtime = Time();
	itime = (int)((rtime - (int)rtime) * 10000);
	sprintf(nkey,"%04d.%04d",myid,itime);
}
int HTSWsessionid(int myid,PCStr(keynam),PCStr(key))
{	int dgid,ssid;

	dgid = ssid = 0;
	sscanf(key,"%d.%d",&dgid,&ssid);
	if( dgid == myid ){
		return ssid;
	}
	if( dgid != 0 ){
		sv1log("## not issued by me: %s=%s\n",keynam,key);
	}
	return 0;
}
void HTSWgetControlCookie(HtSwitch *sw,PCStr(cookie),int myid)
{	const char *dp;
	int dgid;
	CStr(label,64);

	sw->s_ison = 0;
	sw->s_key[0] = 0;
	sw->s_doset = 0;

	sprintf(label,"Key-%s=",sw->s_reqcmd);
	if( dp = strcasestr(cookie,label) ){
		wordscanY(dp+strlen(label),AVStr(sw->s_key),sizeof(sw->s_key),"^;");
		sw->s_ison = HTSWsessionid(myid,sw->s_reqcmd,sw->s_key);
	}
	sw->s_beon = sw->s_ison;
}
void HTSWdefaultControl(HtSwitch *sw)
{
	if( sw->s_ison ){
		sw->s_doset = 1;
		sw->s_beon = 1;
	}else{
		sw->s_doset = 1;
		sw->s_beon = 0;
	}
}
int HTSWswitchControl(HtSwitch *sw,PCStr(cmd),PCStr(ukey))
{	HtSwitch *cntrl = sw->s_ctlsw;
	const char *mycmd;
	int len;

	mycmd = sw->s_reqcmd;
	len = strlen(mycmd);

	if( streq(cmd,mycmd) && sw->s_ison ){
		if( ukey[0] && streq(ukey,sw->s_key) )
			return SW_RET;
		else	return SW_GOT;
	}else
	if( strneq(cmd,"ON-",3)  && streq(cmd+3,mycmd) && cntrl->s_ison ){
		sw->s_doset = 1;
		sw->s_beon = 1;
		return SW_SET;
	}else
	if( strneq(cmd,"OFF-",4) && streq(cmd+4,mycmd) && cntrl->s_ison ){
		sw->s_doset = 1;
		sw->s_beon = 0;
		return SW_SET;
	}
	return SW_MIS;
}
void HTTP_readReqBody(Connection *Conn,FILE *fc);
const char *HTTP_originalReqBody(Connection *Conn);
void HTSWgetreq(Connection *Conn,PCStr(query),FILE *fc,PVStr(scmd),int csiz,PVStr(ukey),int ksiz,PCStr(cookie))
{	const char *dp;
	CStr(postbody,256);
	const char *qb;

	dp = wordscanY(query,AVStr(scmd),csiz,"^=&");
	setVStrEnd(ukey,0);

	qb = HTTP_originalReqBody(Conn);
	if( *qb == 0 ){
		HTTP_readReqBody(Conn,fc);
		qb = HTTP_originalReqBody(Conn);
	}
	strcpy(postbody,qb);
	/*
	fgetsBuffered(AVStr(postbody),sizeof(postbody),fc);
	*/

	if( postbody[0] ){
		if( dp = strpbrk(postbody,"\r\n") )
			truncVStr(dp);
		sv1log("## POST body[%s]\n",postbody);
		if( strchr(postbody,'=') ){
			dp = wordscanY(postbody,AVStr(scmd),csiz,"^=&");
			if( *dp == '=' )
				wordscanY(dp+1,AVStr(ukey),ksiz,"^+&");
		}
	}
	sv1log("## cmd[%s] ukey[%s] Cookie[%s]\n",scmd,ukey,cookie);
}
void toFullUrl(Connection *Conn,PCStr(urlself),PVStr(fullurlself),int size);
int HTSWresponse(Connection *Conn,HtSwitch *swv[],PCStr(query),PCStr(cookie),FILE *fc,FILE *tc,PCStr(urlself),PCStr(urlrealm))
{	HtSwitch *sw,*cntrl;
	CStr(cmd,32);
	CStr(ukey,32);
	CStr(nkey,32);
	CStr(fullurlself,256);
	int myid,posted,swi;

	cntrl = swv[0];

	myid = HTSWmyid();
	HTSWgetreq(Conn,query,fc,AVStr(cmd),sizeof(cmd),AVStr(ukey),sizeof(ukey),cookie);
	if( ukey[0] ){
		if( HTSWsessionid(myid,"Key-request",ukey) == 0 )
			ukey[0] = 0;
	}

	for( swi = 0; sw = swv[swi]; swi++ )
		HTSWgetControlCookie(sw,cookie,myid);

	cntrl->s_doset = 1;
	cntrl->s_beon = 1;
	posted = 0;
	for( swi = 1; sw = swv[swi]; swi++ ){
		switch( HTSWswitchControl(sw,cmd,ukey) ){
			case SW_RET: return 0;
			case SW_GOT: goto ViewerControl;
			case SW_SET: posted = 1; goto putResponse;
		}
	}

ViewerControl:
	for( swi = 1; sw = swv[swi]; swi++ )
		HTSWdefaultControl(sw);

putResponse:
	HTSWnewkey(AVStr(nkey),myid);

	if( posted ){
		toFullUrl(Conn,urlself,AVStr(fullurlself),sizeof(fullurlself));
		fprintf(tc,"HTTP/1.0 302\r\n");
		fprintf(tc,"Location: %s?%s=%s\r\n",
			fullurlself,cntrl->s_requrl,nkey);
		Conn->statcode = 302;
	}else{
		fprintf(tc,"HTTP/1.0 200\r\n");
		Conn->statcode = 200;
	}
	for( swi = 0; sw = swv[swi]; swi++ ){
		HTSWputControlCookie(tc,sw,urlrealm,nkey);
	}

	fprintf(tc,"Content-Type: text/html\r\n");
	fprintf(tc,"Pragma: no-cache\r\n");
	fprintf(tc,"\r\n");

	fprintf(tc,"<TITLE>%s (%s)</TITLE>\r\n",cntrl->s_swdesc,urlrealm);
	fprintf(tc,"<H3>%s</H3>\r\n",cntrl->s_swdesc);

	for( swi = 1; sw = swv[swi]; swi++ ){
		HTSWputControlForm(tc,sw,urlself,nkey);
	}

	fprintf(tc,"<BIG>");
	fprintf(tc,"<A HREF=%s>[Return]</A>\r\n",urlself);
	fprintf(tc,"</BIG><HR>");
	putFrogForDeleGate(Conn,tc,"");
	return 1;
}
void toFullUrl(Connection *Conn,PCStr(urlself),PVStr(fullurlself),int size)
{	const char *proto;
	int rem;

	if( CLNT_PROTO[0] )
		proto = CLNT_PROTO;
	else	proto = "http";
	sprintf(fullurlself,"%s://",proto);
	HTTP_ClientIF_HP(Conn,TVStr(fullurlself));
	rem = size - strlen(fullurlself);
	wordscanX(urlself,TVStr(fullurlself),rem);
}