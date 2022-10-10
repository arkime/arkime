#!/bin/bash
name="<%= name %>"
################################################################################
if [ -d "/etc/systemd" ] && [ -x "/bin/systemctl" ]; then
    ln -sf /opt/$name/etc/arkimecapture.systemd.service /etc/systemd/system/arkimecapture.service
    ln -sf /opt/$name/etc/arkimecont3xt.systemd.service /etc/systemd/system/arkimecont3xt.service
    ln -sf /opt/$name/etc/arkimeparliament.systemd.service /etc/systemd/system/arkimeparliament.service
    ln -sf /opt/$name/etc/arkimeviewer.systemd.service /etc/systemd/system/arkimeviewer.service
    ln -sf /opt/$name/etc/arkimewise.systemd.service /etc/systemd/system/arkimewise.service
    /usr/bin/systemctl daemon-reload
    echo "Arkime systemd files linked"
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
}
EOF
else
    echo "Not installing Arkime logrotate /etc/logrotate.d/$name"
fi

################################################################################
echo "READ /opt/$name/README.txt and RUN /opt/$name/bin/Configure"
