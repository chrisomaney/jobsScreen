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
#
# Import: System → Scripts → or paste into Terminal.

# --- MQTT broker (edit these three lines) ---
/iot mqtt brokers
:if ([:len [find where name="ha-mosquitto"]] = 0) do={
  add name="ha-mosquitto" address="192.168.1.10" port=1883 username="" password="" ssl=no
} else={
  set [find where name="ha-mosquitto"] address="192.168.1.10" port=1883 username="" password="" ssl=no
}

# --- Remove old kids-jobs subscriptions for this broker ---
/iot mqtt subscriptions
:foreach i in=[find where broker="ha-mosquitto" and topic~"^kids/.*/internet\$"] do={
  remove $i
}

# --- One subscription per kid ---
# on = jobs done → resume (allow internet)
# off = jobs left → pause (block internet)

/iot mqtt subscriptions
add broker="ha-mosquitto" topic="kids/xavier/internet" qos=0 on-message={
  :if ($msgData = "on") do={
    /ip kid-control resume xavier
    :log info "kids-jobs: RESUME xavier"
  }
  :if ($msgData = "off") do={
    /ip kid-control pause xavier
    :log info "kids-jobs: PAUSE xavier"
  }
}
add broker="ha-mosquitto" topic="kids/presley/internet" qos=0 on-message={
  :if ($msgData = "on") do={
    /ip kid-control resume presley
    :log info "kids-jobs: RESUME presley"
  }
  :if ($msgData = "off") do={
    /ip kid-control pause presley
    :log info "kids-jobs: PAUSE presley"
  }
}
add broker="ha-mosquitto" topic="kids/jax/internet" qos=0 on-message={
  :if ($msgData = "on") do={
    /ip kid-control resume jax
    :log info "kids-jobs: RESUME jax"
  }
  :if ($msgData = "off") do={
    /ip kid-control pause jax
    :log info "kids-jobs: PAUSE jax"
  }
}
add broker="ha-mosquitto" topic="kids/banx/internet" qos=0 on-message={
  :if ($msgData = "on") do={
    /ip kid-control resume banx
    :log info "kids-jobs: RESUME banx"
  }
  :if ($msgData = "off") do={
    /ip kid-control pause banx
    :log info "kids-jobs: PAUSE banx"
  }
}
add broker="ha-mosquitto" topic="kids/hazel/internet" qos=0 on-message={
  :if ($msgData = "on") do={
    /ip kid-control resume hazel
    :log info "kids-jobs: RESUME hazel"
  }
  :if ($msgData = "off") do={
    /ip kid-control pause hazel
    :log info "kids-jobs: PAUSE hazel"
  }
}

# Optional — create all-day profiles (skip if they already exist):
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
# Check:
# /iot mqtt brokers print
# /iot mqtt subscriptions print detail
# /ip kid-control print
# /log print where message~"kids-jobs"
