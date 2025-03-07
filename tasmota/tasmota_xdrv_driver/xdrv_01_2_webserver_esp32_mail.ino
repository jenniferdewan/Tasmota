/*
  xdrv_01_2_webserver_esp32_mail.ino - Mail client

  SPDX-FileCopyrightText: 2020 Gerhard Mutz and Theo Arends

  SPDX-License-Identifier: GPL-3.0-only
*/

#ifdef ESP32
#ifdef USE_WEBSERVER
#ifdef USE_SENDMAIL
/*********************************************************************************************\
 * ESP32 SendMail works with server port 465 SSL and 587 STARTTLS
 *
 * sendmail [server:port:user:passwd:from:to:subject] body
 *
 * sendmail [*:*:*:*:*:to:subject] data uses defines from user_config_overwrite
 * #define EMAIL_USER "user"
 * #define EMAIL_PASSWORD "passwd"
 * #define EMAIL_FROM "mr.x@gmail.com"
 * #define EMAIL_SERVER "smtp.gmail.com"
 * #define EMAIL_PORT 465
 *
 * if email body consist of a single * and scripter is present
 * and a section >m is found, the lines in this section (until #) are sent as email body
\*********************************************************************************************/

#include <ESP_Mail_Client.h>

// #define DEBUG_EMAIL_PORT    // Enable debugging

#ifndef SEND_MAIL32_MINRAM
#undef SEND_MAIL32_MINRAM
#define SEND_MAIL32_MINRAM 70*1024
#endif

#define xPSTR(a) a

#define MAX_ATTCHMENTS 8

void script_send_email_body(void(*func)(char *));

/*********************************************************************************************/

char *attachments[MAX_ATTCHMENTS];
uint8_t num_attachments;
String html_content = "";
SMTP_Message *email_mptr;
SMTPSession *smtp;

//SMTPSession smtp;
void smtpCallback(SMTP_Status status);

uint16_t SendMail(char *buffer) {
  // return if not enough memory
  uint32_t mem = ESP_getFreeHeap();
  if (mem < SEND_MAIL32_MINRAM) { return 4; }

  while (*buffer==' ') buffer++;
  if (*buffer != '[') { return 1; }

  buffer++;
  char *endcmd = strchr(buffer, ']');
  if (!endcmd) { return 1; }

  // copy params
  uint16_t blen = (uint32_t)endcmd - (uint32_t)buffer;
  char *oparams = (char*)calloc(blen+2, 1);
  if (!oparams) { return 4; }

  uint16_t status = 1;
  char *params = oparams;
  strncpy(oparams, buffer, blen+2);
  oparams[blen] = 0;
  const char *cmd = endcmd + 1;

#ifdef DEBUG_EMAIL_PORT
  AddLog(LOG_LEVEL_INFO, PSTR("MAI: Size %d"), blen);
#endif

  for (uint32_t cnt = 0; cnt < MAX_ATTCHMENTS; cnt++) {
    attachments[cnt] = 0;
  }
  num_attachments = 0;

  smtp = new SMTPSession();
  if (!smtp) {
    if (oparams) { free(oparams); }
    return 4;
  }

  const char *mserv = strtok(params, ":");
  if (mserv) {
    // port
    const char *pstr = strtok(NULL, ":");
    if (pstr) {
      uint16_t port;
    #ifdef EMAIL_PORT
      port = (*pstr == '*') ? EMAIL_PORT : port = atoi(pstr);
    #else
      port = atoi(pstr);
    #endif
      const char *user = strtok(NULL, ":");
      if (user) {
        const char *passwd = strtok(NULL, ":");
        if (passwd) {
          const char *from = strtok(NULL, ":");
          if (from) {
            const char *to = strtok(NULL, ":");
            if (to) {
              const char *subject = strtok(NULL, "]");
              if (subject) {
#ifdef EMAIL_USER
                if (*user == '*') { user = xPSTR(EMAIL_USER); }
#endif
#ifdef EMAIL_PASSWORD
                if (*passwd == '*') { passwd = xPSTR(EMAIL_PASSWORD); }
#endif
#ifdef EMAIL_SERVER
                if (*mserv == '*') { mserv = xPSTR(EMAIL_SERVER); }
#endif

#ifdef DEBUG_EMAIL_PORT
                AddLog(LOG_LEVEL_INFO, PSTR("MAI: %s, %d, %s, %s"), mserv, port, user, passwd);
#endif

#ifdef EMAIL_FROM
                if (*from == '*') { from = xPSTR(EMAIL_FROM); }
#endif

#ifdef DEBUG_EMAIL_PORT
                AddLog(LOG_LEVEL_INFO, PSTR("MAI: %s, %s, %s, %s"), from, to, subject, cmd);
#endif

#ifdef DEBUG_EMAIL_PORT
                smtp->debug(true);
#else
                smtp->debug(false);
#endif

              //  smtp->callback(smtpCallback);

                SMTP_Message message;
                email_mptr = &message;
                message.clearRecipients();
                message.clearCc();
                message.clearBcc();

                ESP_Mail_Session session;
                session.server.host_name = mserv;
                session.server.port = port;
                session.login.email = user;
                session.login.password = passwd;
                session.login.user_domain = "googlemail.com";

                message.enable.chunking = true;
                char sname[32];
                strlcpy(sname, SettingsText(SET_FRIENDLYNAME1), sizeof(sname));
                message.sender.name = sname;
                message.sender.email = from;
                message.subject = subject;
                message.addRecipient("user1", to);
                message.html.charSet = "utf-8";
                message.text.charSet = "utf-8";
                message.text.transfer_encoding = Content_Transfer_Encoding::enc_base64;
                message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
                //message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;
                message.response.notify = esp_mail_smtp_notify_failure;
                message.html.charSet = "us-ascii";
                message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
                message.addHeader("Message-ID: <user1@gmail.com>");

/*-------------------------------------------------------------------------------------------*/

#ifdef USE_SCRIPT
                if (*cmd == '*' && *(cmd + 1) == 0) {
                  script_send_email_body(send_message_txt);
                } else {
                  html_content += cmd;
                  message.html.content = html_content.c_str();
                }
#else
                html_content += cmd;
                message.html.content = html_content.c_str();
#endif  // USE_SCRIPT

/*-------------------------------------------------------------------------------------------*/

                /* Connect to server with the session config */
                delay(0);
                if (smtp->connect(&session)) {
                  /* Start sending the Email and close the session */
                  delay(0);
                  if (!MailClient.sendMail(smtp, &message, true)) {
                    Serial.println("Error sending Email, " + smtp->errorReason());
                  } else {
                    status = 0;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (smtp) { delete smtp; }
  for (uint32_t cnt = 0; cnt < MAX_ATTCHMENTS; cnt++) {
    if (attachments[cnt]) {
      free(attachments[cnt]);
      attachments[cnt] = 0;
    }
  }
  html_content = "";
  if (oparams) { free(oparams); }
  return status;
}

/*********************************************************************************************/

#ifdef USE_SCRIPT

void send_message_txt(char *txt) {
  if (*txt == '@') {
    txt++;
    attach_File(txt);
  } else if (*txt == '&') {
    txt++;
    attach_Array(txt);
  } else if (*txt == '$') {
    txt++;
#if defined(ESP32) && defined(USE_WEBCAM)
    if (num_attachments < MAX_ATTCHMENTS) {
      attachments[num_attachments] = (char*)malloc(32);
      uint32_t cnt;
      uint8_t *buff;
      uint32_t len, picmax;
      picmax = WcGetPicstore(-1, 0);
      cnt = *txt&7;
      if (cnt < 1 || cnt > picmax) cnt = 1;
      len = WcGetPicstore(cnt - 1, &buff);
      if (len) {
        sprintf(attachments[num_attachments], "img_%1d.jpg", cnt);
        attach_Data(attachments[num_attachments], buff, len);
      }
      num_attachments++;
    }
#endif
  } else {
    html_content += txt;
    email_mptr->html.content  = html_content.c_str();
  }
}

void attach_File(char *path) {
  SMTP_Attachment att;
  if (num_attachments < MAX_ATTCHMENTS) {
    attachments[num_attachments] = (char*)malloc(32);
    strcpy(attachments[num_attachments], path);

    char *cp = attachments[num_attachments];
    att.file.path = cp;
    if (*cp == '/') {
      cp++;
    }
    att.descr.filename = cp;
    att.descr.mime = "application/octet-stream"; //binary data
    att.file.storage_type = esp_mail_file_storage_type_univ;
    att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
    email_mptr->addAttachment(att);
    email_mptr->resetAttachItem(att);
    num_attachments++;
  }
}

float *get_array_by_name(char *name, uint16_t *alen);
void flt2char(float num, char *nbuff);

void attach_Array(char *aname) {
  float *array = 0;
  uint16_t alen;
  String ttstr = "";
  array = get_array_by_name(aname, &alen);
  if (array && alen) {
    float *fp = array;
    for (uint32_t cnt = 0; cnt < alen; cnt++) {
      // export array as tab gelimited text
      char nbuff[16];
      flt2char(*fp++, nbuff);
      if (cnt < (alen - 1)) {
        strcat(nbuff, "\t");
      } else {
        strcat(nbuff, "\n");
      }
      ttstr += nbuff;
    }

    if (num_attachments < MAX_ATTCHMENTS) {
      attachments[num_attachments] = (char*)malloc(ttstr.length() + 1 + 32);
      strcpy(attachments[num_attachments] + 32, ttstr.c_str());
      sprintf(attachments[num_attachments], "%s.txt", aname);
      attach_Data(attachments[num_attachments], (uint8_t*)attachments[num_attachments]+32, ttstr.length());
      num_attachments++;
    }
  } else {
    //g_client->print(F("\r\n\r\narray not found!\r\n"));
  }
}

void attach_Data(char *name, uint8_t *buff, uint32_t len) {
  SMTP_Attachment att;
  att.descr.filename = name;
  att.descr.mime = "application/octet-stream";
  att.blob.data = buff;
  att.blob.size = len;
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
  email_mptr->addAttachment(att);
  email_mptr->resetAttachItem(att);
}

#endif  // USE_SCRIPT

/*********************************************************************************************/

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()) {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp->sendingResult.size(); i++) {
      /* Get the result item */
      SMTP_Result result = smtp->sendingResult.getItem(i);
      localtime_r(&result.timesstamp, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}

void Tasmota_print(const char *txt) {
#ifdef DEBUG_EMAIL_PORT
  AddLog(LOG_LEVEL_INFO, PSTR("MAI: %s"),txt);
#endif
}

#endif  // USE_SENDMAIL
#endif  // USE_WEBSERVER
#endif  // ESP32