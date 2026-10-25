# MikroTik — COMMON Kid Control profile (Xavier OR Presley unlock)
#
# Panel publishes retained kids/common/internet = on|off
# when Xavier and/or Presley finish their own required jobs.
#
# Profile name must be exactly: COMMON
# Paste from Terminal root (/). Broker ha-mosquitto must already exist.

/iot mqtt subscriptions
:foreach i in=[find where broker="ha-mosquitto" and topic="kids/common/internet"] do={
  remove $i
}

/iot mqtt subscriptions
add broker="ha-mosquitto" topic="kids/common/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: COMMON got [" . $d . "]")
  :if ($d = "on") do={
    /ip kid-control resume [find where name="COMMON"]
    :log info "kids-jobs: RESUME COMMON"
  }
  :if ($d = "off") do={
    /ip kid-control pause [find where name="COMMON"]
    :log info "kids-jobs: PAUSE COMMON"
  }
}

/iot mqtt disconnect broker="ha-mosquitto"
:delay 2s
/iot mqtt connect broker="ha-mosquitto"

# Check:
# /iot/mqtt/subscriptions print detail
# /log print where message~"COMMON"
