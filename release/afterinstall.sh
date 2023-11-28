#!/bin/bash
name="<%= name %>"

# Running from make config doesn't fill in template
if [[ "${name::1}" == "<" ]]; then
    name="arkime"
fi

################################################################################
if [ -d "/etc/systemd" ] && [ -x "/bin/systemctl" ]; then
    /bin/unlink /etc/systemd/system/arkimecapture.service >/dev/null 2>&1
    /bin/unlink /etc/systemd/system/arkimecont3xt.service >/dev/null 2>&1
    /bin/unlink /etc/systemd/system/arkimeparliament.service >/dev/null 2>&1
    /bin/unlink /etc/systemd/system/arkimeviewer.service >/dev/null 2>&1
    /bin/unlink /etc/systemd/system/arkimewise.service >/dev/null 2>&1

    /bin/cp -f /opt/$name/etc/arkimecapture.systemd.service /etc/systemd/system/arkimecapture.service
    /bin/cp -f /opt/$name/etc/arkimecont3xt.systemd.service /etc/systemd/system/arkimecont3xt.service
    /bin/cp -f /opt/$name/etc/arkimeparliament.systemd.service /etc/systemd/system/arkimeparliament.service
    /bin/cp -f /opt/$name/etc/arkimeviewer.systemd.service /etc/systemd/system/arkimeviewer.service
    /bin/cp -f /opt/$name/etc/arkimewise.systemd.service /etc/systemd/system/arkimewise.service
    /usr/bin/systemctl daemon-reload
    echo "Arkime systemd files copied"
fi

################################################################################
if [ -d "/etc/logrotate.d" ] && [ ! -f "/etc/logrotate.d/$name" ]; then
    echo "Installing logrotate /etc/logrotate.d/$name to delete files after 14 days"
    cat << EOF > "/etc/logrotate.d/$name"
/opt/$name/logs/capture.log
/opt/$name/logs/viewer.log
/opt/$name/logs/wise.log
/opt/$name/logs/cont3xt.log
/opt/$name/logs/parliament.log {
    daily
    rotate 14
    notifempty
    copytruncate
    missingok
}
EOF
else
    echo "Not installing Arkime logrotate /etc/logrotate.d/$name"
fi

################################################################################
echo "READ /opt/$name/README.txt and RUN /opt/$name/bin/Configure"
