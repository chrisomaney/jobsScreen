# MikroTik — COMMON Kid Control profile (Xavier OR Presley unlock)
#
# Panel publishes retained kids/common/internet = on|off
# when Xavier and/or Presley finish their own required jobs.
#
# Profile name must be exactly: common  (lowercase)
# Paste from Terminal root (/). Broker ha-mosquitto must already exist.
#
# Raise parallel script limit — retained kids/*/internet all fire on reconnect;
# default/low limits (e.g. 3) cause: "Ignoring script execution... Too many
# broker scripts already running (3/3)".
/iot mqtt brokers set [find where name="ha-mosquitto"] parallel-scripts-limit=20

/iot mqtt subscriptions
:foreach i in=[find where broker="ha-mosquitto" and topic="kids/common/internet"] do={
  remove $i
}

/iot mqtt subscriptions
add broker="ha-mosquitto" topic="kids/common/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: common got [" . $d . "]")
  :if ($d = "on") do={
    /ip kid-control resume [find where name="common"]
    :log info "kids-jobs: RESUME common"
  }
  :if ($d = "off") do={
    /ip kid-control pause [find where name="common"]
    :log info "kids-jobs: PAUSE common"
  }
}

/iot mqtt disconnect broker="ha-mosquitto"
:delay 2s
/iot mqtt connect broker="ha-mosquitto"

# Check:
# /iot/mqtt/subscriptions print detail
# /log print where message~"common"
