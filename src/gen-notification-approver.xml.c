const char * _notification_approver = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<node name=\"/\">\n"
"	<interface name=\"org.ayatana.StatusNotifierApprover\">\n"
"\n"
"<!-- Methods -->\n"
"		<method name=\"ApproveItem\">\n"
"			<!-- KSNI ID -->\n"
"			<arg type=\"s\" name=\"id\" direction=\"in\" />\n"
"			<!-- KSNI Category -->\n"
"			<arg type=\"s\" name=\"category\" direction=\"in\" />\n"
"			<!-- Application PID -->\n"
"			<arg type=\"u\" name=\"pid\" direction=\"in\" />\n"
"			<!-- Application DBus Address -->\n"
"			<arg type=\"s\" name=\"address\" direction=\"in\" />\n"
"			<!-- Application DBus Path for KSNI interface -->\n"
"			<arg type=\"o\" name=\"path\" direction=\"in\" />\n"
"			<!-- So, what do you think? -->\n"
"			<arg type=\"b\" name=\"approved\" direction=\"out\" />\n"
"		</method>\n"
"\n"
"<!-- Signals -->\n"
"		<signal name=\"ReviseJudgement\">\n"
"			<arg type=\"b\" name=\"approved\" direction=\"out\" />\n"
"			<arg type=\"s\" name=\"address\" direction=\"out\" />\n"
"			<arg type=\"o\" name=\"path\" direction=\"out\" />\n"
"		</signal>\n"
"\n"
"	</interface>\n"
"</node>\n"
;
