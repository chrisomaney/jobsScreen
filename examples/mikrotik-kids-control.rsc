# Kids Jobs Panel → MikroTik Kid Control (IoT MQTT)
#
# Flow:
#   Panel publishes kids/{slug}/internet = on|off  (retained)
#   MikroTik IoT subscribes → resume (on) or pause (off) that Kid Control profile
#
# Before you paste:
#   • IoT package installed
#   • Kid Control profiles named: xavier, presley, jax, banx, hazel
#   • Each kid's devices attached under Kid Control → Devices
#   • Profiles allow internet when resumed (e.g. all-day windows)
#   • Edit address / username / password below to match Mosquitto
#   • Mosquitto user can SUBSCRIBE to kids/+/internet
#
# Import: paste into Terminal from root (/). Always use leading /.

# --- MQTT broker (edit address / username / password) ---
# auto-connect=yes is required — default is no, so the link drops after a few
# minutes and on-message (pause/resume) silently stops until you reconnect.
/iot mqtt brokers
:if ([:len [find where name="ha-mosquitto"]] = 0) do={
  add name="ha-mosquitto" address="192.168.1.10" port=1883 username="" password="" ssl=no auto-connect=yes keep-alive=60 client-id="mikrotik-kids-jobs"
} else={
  set [find where name="ha-mosquitto"] address="192.168.1.10" port=1883 username="" password="" ssl=no auto-connect=yes keep-alive=60 client-id="mikrotik-kids-jobs"
}

# --- Remove old kids-jobs subscriptions for this broker ---
/iot mqtt subscriptions
:foreach i in=[find where broker="ha-mosquitto" and topic~"^kids/.*/internet\$"] do={
  remove $i
}

# --- One subscription per kid ---
# Logs every payload (even unknown) so you can see if MQTT arrives.
# Matches "on"/"off" with optional whitespace. Uses find-by-name for profiles.

/iot mqtt subscriptions
add broker="ha-mosquitto" topic="kids/xavier/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: xavier got [" . $d . "]")
  :if ($d~"on") do={
    /ip kid-control resume [find where name="xavier"]
    :log info "kids-jobs: RESUME xavier"
  }
  :if ($d~"off") do={
    /ip kid-control pause [find where name="xavier"]
    :log info "kids-jobs: PAUSE xavier"
  }
}
add broker="ha-mosquitto" topic="kids/presley/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: presley got [" . $d . "]")
  :if ($d~"on") do={
    /ip kid-control resume [find where name="presley"]
    :log info "kids-jobs: RESUME presley"
  }
  :if ($d~"off") do={
    /ip kid-control pause [find where name="presley"]
    :log info "kids-jobs: PAUSE presley"
  }
}
add broker="ha-mosquitto" topic="kids/jax/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: jax got [" . $d . "]")
  :if ($d~"on") do={
    /ip kid-control resume [find where name="jax"]
    :log info "kids-jobs: RESUME jax"
  }
  :if ($d~"off") do={
    /ip kid-control pause [find where name="jax"]
    :log info "kids-jobs: PAUSE jax"
  }
}
add broker="ha-mosquitto" topic="kids/banx/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: banx got [" . $d . "]")
  :if ($d~"on") do={
    /ip kid-control resume [find where name="banx"]
    :log info "kids-jobs: RESUME banx"
  }
  :if ($d~"off") do={
    /ip kid-control pause [find where name="banx"]
    :log info "kids-jobs: PAUSE banx"
  }
}
add broker="ha-mosquitto" topic="kids/hazel/internet" qos=0 on-message={
  :local d [:tostr $msgData]
  :log info ("kids-jobs: hazel got [" . $d . "]")
  :if ($d~"on") do={
    /ip kid-control resume [find where name="hazel"]
    :log info "kids-jobs: RESUME hazel"
  }
  :if ($d~"off") do={
    /ip kid-control pause [find where name="hazel"]
    :log info "kids-jobs: PAUSE hazel"
  }
}

# Re-establish MQTT after adding subscriptions (RouterOS requires this if already connected).
# Reconnect also re-delivers retained kids/*/internet messages.
/iot mqtt disconnect broker="ha-mosquitto"
:delay 2s
/iot mqtt connect broker="ha-mosquitto"

# --- Optional one-shot setup (run once; always use leading / so it works from any menu) ---
# Create all-day profiles (skip if they already exist):
# /ip kid-control add name=xavier  sun=0s-1d mon=0s-1d tue=0s-1d wed=0s-1d thu=0s-1d fri=0s-1d sat=0s-1d
# /ip kid-control add name=presley sun=0s-1d mon=0s-1d tue=0s-1d wed=0s-1d thu=0s-1d fri=0s-1d sat=0s-1d
# /ip kid-control add name=jax     sun=0s-1d mon=0s-1d tue=0s-1d wed=0s-1d thu=0s-1d fri=0s-1d sat=0s-1d
# /ip kid-control add name=banx    sun=0s-1d mon=0s-1d tue=0s-1d wed=0s-1d thu=0s-1d fri=0s-1d sat=0s-1d
# /ip kid-control add name=hazel   sun=0s-1d mon=0s-1d tue=0s-1d wed=0s-1d thu=0s-1d fri=0s-1d sat=0s-1d
#
# Attach devices (replace MACs):
# /ip kid-control device add name="Xavier-iPad" user=xavier mac-address=AA:BB:CC:DD:EE:01
#
# Start locked until jobs are done:
# /ip kid-control pause xavier,presley,jax,banx,hazel
#
# --- Diagnostics (run after a test publish of on/off) ---
# /iot/mqtt/brokers print
#   expect: connected=yes auto-connect=yes
# /iot/mqtt/subscriptions print detail
#   expect: five kids/*/internet rows with on-message set
# /iot/mqtt/subscriptions/recv print
#   expect: recent data=on or data=off for kids/xavier/internet
# /ip/kid-control print
#   expect: a profile named exactly "xavier"
# /log print where message~"kids-jobs"
#   expect: "xavier got [on]" then "RESUME xavier"
#
# Live test from the router (forces a message through the same path):
# /iot mqtt publish broker=ha-mosquitto topic=kids/xavier/internet message=on retain=yes
# /iot mqtt publish broker=ha-mosquitto topic=kids/xavier/internet message=off retain=yes
