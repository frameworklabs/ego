// Copyright (c) 2022, Framework Labs.

#include "pa_plankton.h"

Plankton plankton;

pa_activity_def (Connector) {
    Serial.println("Connectecing to WLAN...");  

    WiFi.setAutoReconnect(true);
    WiFi.begin("ego");
    pa_await (WiFi.isConnected());

    Serial.println("Connectecing to WLAN...DONE");  

    plankton.begin();
} pa_end;

pa_activity_def (AccessPoint) {
    Serial.println("Starting WLAN AccessPoint...");  

    WiFi.softAP("ego", NULL, 1, 1);

    Serial.println("Starting WLAN AccessPoint...DONE");  

    plankton.begin();
} pa_end;

pa_activity_def (Receiver) {
    pa_always {
        plankton.poll();
    } pa_always_end;
} pa_end;
