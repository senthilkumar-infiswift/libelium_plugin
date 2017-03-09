#!/bin/bash
### BEGIN INIT INFO
# Provides:          CloudInfiswift
# Required-Start:    $all
# Required-Stop:     $all
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Starts Cloud sync service
# Description:       Starts Cloud synchronization service
#                    at boot time
### END INIT INFO

#Customize this for your own service!
#Description of the service
DESC="Infiswift cloud synchronization service"
#Lock file name is to signal process started
LOCKFILE="CloudInfiswift"
#Script to start the service (used in start)
SCRIPT_TO_RUN="CloudInfiswift.sh"
#Name of the permanent process (used to stop and status)
PERMANENT_SERVICE="$SCRIPT_TO_RUN"
#processes that need to be stopped (can be more than one, space separated)
PKILL_ARGS="$PERMANENT_SERVICE CloudInfiswift.php"

do_start(){
    if [ -f /var/lock/$LOCKFILE ]
    then
        #There is a lock file!
        echo "$DESC already started."
        exit 1
    else
        #Create lock file
        /usr/bin/touch /var/lock/$LOCKFILE
        #Start service
        /bin/$SCRIPT_TO_RUN &
        echo "$DESC started."
    fi
}

do_stop(){
    if [ -f /var/lock/$LOCKFILE ]
    then
        #Delete the lock file
        sudo rm -f /var/lock/$LOCKFILE
    fi
    #Kill all the processes in the list
    for arg in $PKILL_ARGS
    do
        /usr/bin/pkill -f "$arg"
    done
    echo "$DESC stopped."
}

do_status(){
    if [ -f /var/lock/$LOCKFILE ]
    then
        #If there is lock file, check if the process is running
        pgrep -f "$PERMANENT_SERVICE" && echo "$DESC is running" && exit 0
        #if i am here, pgrep returned null
        echo "$DESC not running"
    else
        #no lock file, no process running
        echo "$DESC not running"
    fi
}

#Main
case "$1" in
    start)
        do_start;
        ;;
    stop)
        do_stop;
        ;;
    restart)
        do_stop;
        do_start;
        ;;
    status)
        do_status;
        ;;
    *)
        echo "Use: $0 start|stop|restart|status" >&2
        exit 3
        ;;
esac
exit 0
