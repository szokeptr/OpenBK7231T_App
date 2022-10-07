#include "../new_common.h"
#include "http_fns.h"
#include "../new_pins.h"
#include "../new_cfg.h"
#include "../ota/ota.h"
// Commands register, execution API and cmd tokenizer
#include "../cmnds/cmd_public.h"
#include "../driver/drv_tuyaMCU.h"
#include "../driver/drv_public.h"
#include "../logging/logging.h"
#include "../hal/hal_wifi.h"
#include "../hal/hal_pins.h"
#include "../hal/hal_flashConfig.h"
#include "../logging/logging.h"
#include "../devicegroups/deviceGroups_public.h"
#include "../mqtt/new_mqtt.h"
#include "hass.h"

#ifdef WINDOWS
    // nothing
#elif PLATFORM_BL602

#elif PLATFORM_W800

#elif PLATFORM_XR809
    #include <image/flash.h>
#elif defined(PLATFORM_BK7231N)
    // tuya-iotos-embeded-sdk-wifi-ble-bk7231n/sdk/include/tuya_hal_storage.h
    #include "tuya_hal_storage.h"
    #include "BkDriverFlash.h"
#else
    // REALLY? A typo in Tuya SDK? Storge?
    // tuya-iotos-embeded-sdk-wifi-ble-bk7231t/platforms/bk7231t/tuya_os_adapter/include/driver/tuya_hal_storge.h
    #include "tuya_hal_storge.h"
    #include "BkDriverFlash.h"
#endif


/*
function send_ha_disc(){
	var xhr = new XMLHttpRequest();
    xhr.open("GET", "/ha_discovery?prefix="+document.getElementById("ha_disc_topic").value, false);
	xhr.onload = function() {
	  if (xhr.status === 200) {
	    alert("MQTT discovery queued");
	  }
      else if (xhr.status === 404) {
		alert("Error invoking ha_discovery");
	  }
	}
	xhr.onerror = function() {
	  alert("Error invoking ha_discovery");
	}
	xhr.send();
}
*/
const char HomeAssistantDiscoveryScript[] = "<script>function send_ha_disc(){var xhr=new XMLHttpRequest();xhr.open(\"GET\",\"/ha_discovery?prefix=\"+document.getElementById(\"ha_disc_topic\").value,false);xhr.onload=function(){if(xhr.status===200){alert(\"MQTT discovery queued\")}else if(xhr.status===404){alert(\"Error invoking ha_discovery\")}};xhr.onerror=function(){alert(\"Error invoking ha_discovery\")};xhr.send()}</script>";

typedef struct template_s {
	void (*setter)();
	const char *name;
} template_t;

template_t g_templates [] = {
	{ Setup_Device_Empty, "Empty"},
	// BK7231N devices
	{ Setup_Device_BK7231N_CB2S_QiachipSmartSwitch, "[BK7231N][CB2S] QiaChip Smart Switch"},
	{ Setup_Device_BK7231N_KS_602_TOUCH, "[BK7231N] KS 602 Touch Switch US"},
	{ Setup_Device_Aubess_Mini_Smart_Switch_16A, "[BK7231N] Aubess Mini Smart Switch 16A"},
	// BK7231T devices
	{ Setup_Device_BK7231T_WB2S_QiachipSmartSwitch, "[BK7231T][WB2S] QiaChip Smart Switch"},
	{ Setup_Device_TuyaWL_SW01_16A, "WL SW01 16A"},
	{ Setup_Device_TuyaSmartLife4CH10A, "Smart Life 4CH 10A"},
	{ Setup_Device_IntelligentLife_NF101A, "Intelligent Life NF101A"},
	{ Setup_Device_TuyaLEDDimmerSingleChannel, "Tuya LED Dimmer Single Channel PWM WB3S"},
	{ Setup_Device_CalexLEDDimmerFiveChannel, "Calex RGBWW LED Dimmer Five Channel PWM BK7231S"},
	{ Setup_Device_CalexPowerStrip_900018_1v1_0UK, "Calex UK power strip 900018.1 v1.0 UK"},
	{ Setup_Device_ArlecCCTDownlight, "Arlec CCT LED Downlight ALD029CHA"},
	{ Setup_Device_NedisWIFIPO120FWT_16A, "Nedis WIFIPO120FWT SmartPlug 16A"},
	{ Setup_Device_NedisWIFIP130FWT_10A, "Nedis WIFIP130FWT SmartPlug 10A"},
	{ Setup_Device_BK7231T_Raw_PrimeWiFiSmartOutletsOutdoor_CCWFIO232PK, "Prime SmartOutlet Outdoor 2x Costco"},
	{ Setup_Device_EmaxHome_EDU8774, "Emax Home EDU8774 SmartPlug 16A"},
	{ Setup_Device_BK7231N_TuyaLightBulb_RGBCW_5PWMs, "Tuya E27 LED RGBCW 5PWMs BK7231N"},
	{ Setup_Device_TuyaSmartPFW02G, "Tuya Smart PFW02-G"},
    { Setup_Device_AvatarASL04, "Avatar ASL04 5v LED strip"},
    { Setup_Device_BL602_MagicHome_IR_RGB_LedStrip, "BL602 Magic Home LED RGB IR Strip"},
    { Setup_Device_Sonoff_MiniR3, "Sonoff MiniR3"},
    { Setup_Device_WiFi_DIY_Switch_WB2S_ZN268131, "WB2S WiFi DIY Switch ZN268131"},
	{ Setup_Device_DS_102_1Gang_WB3S, "DS-102 1 Gang Switch"},
	{ Setup_Device_DS_102_2Gang_WB3S, "DS-102 2 Gang Switch"},
	{ Setup_Device_DS_102_3Gang_WB3S, "DS-102 3 Gang Switch"},
    { Setup_Device_TuyaSmartWIFISwith_4Gang_CB3S, "[BK7231N][CB3S] Tuya Smart Wifi Switch 4 Gang"},
    { Setup_Device_BK7231N_CB2S_LSPA9_BL0942, "[BK7231N][CB2S] LSPA9 power metering plug BL0942 version"},
    { Setup_Device_LSC_Smart_Connect_Plug_CB2S, "[BK7231N][CB2S] LSC Smart Connect Plug"},
	{ Setup_Device_BK7231T_Gosund_Switch_SW5_A_V2_1, "BK7231T Gosund Smart Switch SW5-A-V2.1"},
	{ Setup_Device_13A_Socket_CB2S, "BK7231N CB2S 13A Aliexpress socket"},
    { Setup_Device_Deta_Smart_Double_Power_Point_6922HA_Series2, "BK7231T DETA SMART Double Power Point 6922HA-Series 2"},
    { Setup_Device_ArlecRGBCCTDownlight, "Arlec RGB+CCT LED Downlight ALD092RHA"},
    { Setup_Device_CasaLifeCCTDownlight, "CasaLife CCT LED Downlight SMART-AL2017-TGTS"},
    { Setup_Device_Enbrighten_WFD4103, "Enbrighten WFD4103 WiFi Switch BK7231T WB2S"} 
};

int g_total_templates = sizeof(g_templates)/sizeof(g_templates[0]);

unsigned char hexdigit( char hex ) {
    return (hex <= '9') ? hex - '0' :
                          toupper((unsigned char)hex) - 'A' + 10 ;
}

unsigned char hexbyte( const char* hex ) {
    return (hexdigit(*hex) << 4) | hexdigit(*(hex+1)) ;
}

int http_fn_empty_url(http_request_t *request) {
    poststr(request,"HTTP/1.1 302 OK\nLocation: /index\nConnection: close\n\n");
	poststr(request, NULL);
    return 0;
}

void postFormAction(http_request_t *request, char *action, char *value){
    //"<form action=\"cfg_pins\"><input type=\"submit\" value=\"Configure Module\"/></form>"
    hprintf128(request,"<form action=\"%s\"><input type=\"submit\" value=\"%s\"/></form>", action, value);
}

int http_fn_testmsg(http_request_t *request) {
    poststr(request,"This is just a test msg\n\n");
	poststr(request, NULL);
    return 0;

}

int http_fn_index(http_request_t *request) {
    int j, i;
	char tmpA[128];
	int bRawPWMs;
	int forceShowRGBCW;

	bRawPWMs = CFG_HasFlag(OBK_FLAG_LED_RAWCHANNELSMODE);
	forceShowRGBCW = CFG_HasFlag(OBK_FLAG_LED_FORCESHOWRGBCWCONTROLLER);

    // use ?state URL parameter to only request current state
    if(!http_getArg(request->url, "state", tmpA, sizeof(tmpA))) {
    http_setup(request, httpMimeTypeHTML);
        http_html_start(request, NULL);

        poststr(request, "<div id=\"changed\">");
    if(http_getArg(request->url,"tgl",tmpA,sizeof(tmpA))) {
        j = atoi(tmpA);
		if(j == SPECIAL_CHANNEL_LEDPOWER) {
			hprintf128(request,"<h3>Toggled LED power!</h3>",j);
		} else {
			hprintf128(request,"<h3>Toggled %i!</h3>",j);
		}
        CHANNEL_Toggle(j);
    }
    if(http_getArg(request->url,"on",tmpA,sizeof(tmpA))) {
        j = atoi(tmpA);
        hprintf128(request,"<h3>Enabled %i!</h3>",j);
        CHANNEL_Set(j,255,1,0);
    }
    if(http_getArg(request->url,"rgb",tmpA,sizeof(tmpA))) {
        hprintf128(request,"<h3>Set RGB to %s!</h3>",tmpA);
		LED_SetBaseColor(0,"led_basecolor",tmpA,0);
    }
	
    if(http_getArg(request->url,"off",tmpA,sizeof(tmpA))) {
        j = atoi(tmpA);
        hprintf128(request,"<h3>Disabled %i!</h3>",j);
        CHANNEL_Set(j,0,1,0);
    }
    if(http_getArg(request->url,"pwm",tmpA,sizeof(tmpA))) {
        int newPWMValue = atoi(tmpA);
        http_getArg(request->url,"pwmIndex",tmpA,sizeof(tmpA));
        j = atoi(tmpA);
		if(j == SPECIAL_CHANNEL_TEMPERATURE) {
			hprintf128(request,"<h3>Changed Temperature to %i!</h3>",newPWMValue);
		} else {
			hprintf128(request,"<h3>Changed pwm %i to %i!</h3>",j,newPWMValue);
		}
        CHANNEL_Set(j,newPWMValue,1,0);
    }
    if(http_getArg(request->url,"dim",tmpA,sizeof(tmpA))) {
        int newDimmerValue = atoi(tmpA);
        http_getArg(request->url,"dimIndex",tmpA,sizeof(tmpA));
		j  = atoi(tmpA);
		if(j == SPECIAL_CHANNEL_BRIGHTNESS) {
			hprintf128(request,"<h3>Changed LED brightness to %i!</h3>",newDimmerValue);
		} else {
			hprintf128(request,"<h3>Changed dimmer %i to %i!</h3>",j,newDimmerValue);
		}
        CHANNEL_Set(j,newDimmerValue,1,0);
    }
    if(http_getArg(request->url,"set",tmpA,sizeof(tmpA))) {
        int newSetValue = atoi(tmpA);
        http_getArg(request->url,"setIndex",tmpA,sizeof(tmpA));
        j = atoi(tmpA);
        hprintf128(request,"<h3>Changed channel %i to %i!</h3>",j,newSetValue);
        CHANNEL_Set(j,newSetValue,1,0);
    }
        if(http_getArg(request->url,"restart",tmpA,sizeof(tmpA))) {
            poststr(request,"<h5> Module will restart soon</h5>");
            RESET_ScheduleModuleReset(3);
        }
        poststr(request, "</div>"); // end div#change
        poststr(request, "<div id=\"state\">"); // replaceable content follows
    }

    poststr(request, "<table width=\"100%\">");
    for (i = 0; i < CHANNEL_MAX; i++) {
        int channelType;

        channelType = CHANNEL_GetType(i);
        if (h_isChannelRelay(i) || channelType == ChType_Toggle) {
            if (i <= 1) {
                hprintf128(request, "<tr>");
            }
            if (CHANNEL_Check(i)) {
                poststr(request, "<td style=\"text-align:center; font-weight:bold; font-size:54px\">ON</td>");
            }
            else {
                poststr(request, "<td style=\"text-align:center; font-size:54px\">OFF</td>");
            }
            if (i == CHANNEL_MAX - 1) {
                poststr(request, "</tr>");
            }
        }
    }
    poststr(request, "</table>");
    
    poststr(request, "<table width=\"100%\">");
    for(i = 0; i < CHANNEL_MAX; i++) {

        int channelType;

		channelType = CHANNEL_GetType(i);
		if(channelType == ChType_Temperature) {
			int iValue;

			iValue = CHANNEL_Get(i);
            poststr(request, "<tr><td>");
            hprintf128(request,"Temperature Channel %i value %i C<br>",i, iValue);
            poststr(request, "</td></tr>");
            
        } else if(channelType == ChType_Temperature_div10) {
			int iValue;
			float fValue;

			iValue = CHANNEL_Get(i);
			fValue = iValue * 0.1f;

            poststr(request, "<tr><td>");
            hprintf128(request,"Temperature Channel %i value %f C<br>",i, fValue);
            poststr(request, "</td></tr>");

		} else  if(channelType == ChType_Humidity) {
			int iValue;

			iValue = CHANNEL_Get(i);

            poststr(request, "<tr><td>");
            hprintf128(request,"Humidity Channel %i value %i Percent<br>",i, iValue);
            poststr(request, "</td></tr>");

		} else if(channelType == ChType_Humidity_div10) {
			int iValue;
			float fValue;

			iValue = CHANNEL_Get(i);
			fValue = iValue * 0.1f;

            poststr(request, "<tr><td>");
            hprintf128(request,"Humidity Channel %i value %f Percent<br>",i, fValue);
            poststr(request, "</td></tr>");

		} else if(channelType == ChType_LowMidHigh) {
			const char *types[]={"Low","Mid","High"};
			int iValue;
			iValue = CHANNEL_Get(i);

            poststr(request, "<tr><td>");
            hprintf128(request,"<p>Select speed:</p><form action=\"index\">");
            hprintf128(request,"<input type=\"hidden\" name=\"setIndex\" value=\"%i\">",i);
			for(j = 0; j < 3; j++) {
				const char *check;
				if(j == iValue)
					check = "checked";
				else
					check = "";
				hprintf128(request,"<input type=\"radio\" name=\"set\" value=\"%i\" onchange=\"this.form.submit()\" %s>%s",j,check,types[j]);
			}
            hprintf128(request,"</form>");
            poststr(request, "</td></tr>");

		} else if(channelType == ChType_OffLowMidHigh || channelType == ChType_OffLowestLowMidHighHighest || channelType == ChType_LowestLowMidHighHighest) {
			const char **types;
			const char *types4[] = {"Off","Low","Mid","High"};
			const char *types6[] = {"Off", "Lowest", "Low", "Mid", "High", "Highest"};
			const char *types5NoOff[] = { "Lowest", "Low", "Mid", "High", "Highest"};
			int numTypes;
			int iValue;
			
			if(channelType == ChType_OffLowMidHigh) {
				types = types4;
				numTypes = 4;
			} else if(channelType == ChType_LowestLowMidHighHighest) {
				types = types5NoOff;
				numTypes = 5;
			} else {
				types = types6;
				numTypes = 6;
			}

			iValue = CHANNEL_Get(i);

            poststr(request, "<tr><td>");
            hprintf128(request,"<p>Select speed:</p><form action=\"index\">");
            hprintf128(request,"<input type=\"hidden\" name=\"setIndex\" value=\"%i\">",i);
			for(j = 0; j < numTypes; j++) {
				const char *check;
				if(j == iValue)
					check = "checked";
				else
					check = "";
				hprintf128(request,"<input type=\"radio\" name=\"set\" value=\"%i\" onchange=\"this.form.submit()\" %s>%s",j,check,types[j]);
			}
            hprintf128(request,"</form>");
            poststr(request, "</td></tr>");

		} else if(channelType == ChType_TextField) {
			int iValue;
			iValue = CHANNEL_Get(i);

            poststr(request, "<tr><td>");
            hprintf128(request,"<p>Change channel %i value:</p><form action=\"index\">",i);
            hprintf128(request,"<input type=\"hidden\" name=\"setIndex\" value=\"%i\">",i);
            hprintf128(request,"<input type=\"number\" name=\"set\" value=\"%i\" onblur=\"this.form.submit()\">",iValue);
            hprintf128(request,"<input type=\"submit\" value=\"Set!\"/></form>");
            hprintf128(request,"</form>");
            poststr(request, "</td></tr>");

		} else if(channelType == ChType_ReadOnly) {
			int iValue;
			iValue = CHANNEL_Get(i);

            poststr(request, "<tr><td>");
            hprintf128(request,"Channel %i = %i",i,iValue);
            poststr(request, "</td></tr>");

        }
        else if (h_isChannelRelay(i) || channelType == ChType_Toggle) {
            if (i <= 1) {
                hprintf128(request, "<tr>");
            }
            const char *c;
            if(CHANNEL_Check(i)) {
                c = "bgrn";
            } else {
                c = "bred";
            }
            poststr(request,"<td><form action=\"index\">");
            hprintf128(request,"<input type=\"hidden\" name=\"tgl\" value=\"%i\">",i);
            hprintf128(request,"<input class=\"%s\" type=\"submit\" value=\"Toggle %i\"/></form></td>",c,i);
            if (i == CHANNEL_MAX-1) {
                poststr(request, "</tr>");
            }
        }
        else if((bRawPWMs&&h_isChannelPWM(i)) || (channelType == ChType_Dimmer)) {
			
        	// PWM and dimmer both use a slider control
        	const char *inputName = h_isChannelPWM(i) ? "pwm" : "dim";
            int pwmValue;

            pwmValue = CHANNEL_Get(i);
            poststr(request, "<tr><td>");
            hprintf128(request,"<form action=\"index\" id=\"form%i\">",i);
            hprintf128(request,"<input type=\"range\" min=\"0\" max=\"100\" name=\"%s\" id=\"slider%i\" value=\"%i\" onchange=\"this.form.submit()\">",inputName,i,pwmValue);
            hprintf128(request,"<input type=\"hidden\" name=\"%sIndex\" value=\"%i\">",inputName,i);
            hprintf128(request,"<input  type=\"submit\" style=\"display:none;\" value=\"Toggle %i\"/></form>",i);
            poststr(request, "</td></tr>");
        }
    }

	if(bRawPWMs == 0 || forceShowRGBCW) {
		int c_pwms;
		int lm;

		lm = LED_GetMode();

		c_pwms = PIN_CountPinsWithRoleOrRole(IOR_PWM, IOR_PWM_n);
		if(forceShowRGBCW) {
			c_pwms = 5;
		}

		if(c_pwms > 0) {
			const char *c;
			if(CHANNEL_Check(SPECIAL_CHANNEL_LEDPOWER)) {
				c = "bgrn";
			} else {
				c = "bred";
			}
			poststr(request, "<tr><td>");
			poststr(request,"<form action=\"index\">");
			hprintf128(request,"<input type=\"hidden\" name=\"tgl\" value=\"%i\">",SPECIAL_CHANNEL_LEDPOWER);
			hprintf128(request,"<input class=\"%s\" type=\"submit\" value=\"Toggle Light\"/></form>",c);
			poststr(request, "</td></tr>");
		}

		if(c_pwms > 0) {
            int pwmValue;
        	const char *inputName;

			inputName = "dim";

            pwmValue = LED_GetDimmer();

			poststr(request, "<tr><td>");
            hprintf128(request,"<h5> LED Dimmer/Brightness </h5>");
            hprintf128(request,"<form action=\"index\" id=\"form%i\">",SPECIAL_CHANNEL_BRIGHTNESS);
            hprintf128(request,"<input type=\"range\" min=\"0\" max=\"100\" name=\"%s\" id=\"slider%i\" value=\"%i\" onchange=\"this.form.submit()\">",inputName,SPECIAL_CHANNEL_BRIGHTNESS,pwmValue);
            hprintf128(request,"<input type=\"hidden\" name=\"%sIndex\" value=\"%i\">",inputName,SPECIAL_CHANNEL_BRIGHTNESS);
            hprintf128(request,"<input  type=\"submit\" style=\"display:none;\" value=\"Toggle %i\"/></form>",SPECIAL_CHANNEL_BRIGHTNESS);
			poststr(request, "</td></tr>");
		}
		if(c_pwms >= 3) {
			char colorValue[16];
			const char *inputName = "rgb";
			const char *activeStr = "";
			if(lm == Light_RGB) {
				activeStr = "[ACTIVE]";
			}

			LED_GetBaseColorString(colorValue);
			poststr(request, "<tr><td>");
            hprintf128(request,"<h5> LED RGB Color %s </h5>",activeStr);
            hprintf128(request,"<form action=\"index\" id=\"form%i\">",SPECIAL_CHANNEL_BASECOLOR);
			hprintf128(request,"<input type=\"color\" name=\"%s\" id=\"color%i\" value=\"#%s\" onchange=\"this.form.submit()\">",inputName,SPECIAL_CHANNEL_BASECOLOR,colorValue);
            hprintf128(request,"<input type=\"hidden\" name=\"%sIndex\" value=\"%i\">",inputName,SPECIAL_CHANNEL_BASECOLOR);
            hprintf128(request,"<input  type=\"submit\" style=\"display:none;\" value=\"Toggle Light\"/></form>");
			poststr(request, "</td></tr>");
		}
		if(c_pwms == 2 || c_pwms == 5) {
			// TODO: temperature slider
			int pwmValue;
        	const char *inputName;
			const char *activeStr = "";
			if(lm == Light_Temperature) {
				activeStr = "[ACTIVE]";
			}

			inputName = "pwm";

            pwmValue = LED_GetTemperature();

			poststr(request, "<tr><td>");
            hprintf128(request,"<h5> LED Temperature Slider %s (cur=%i, min=%i, max=%i) (Cold <--- ---> Warm) </h5>",activeStr,pwmValue,HASS_TEMPERATURE_MIN,HASS_TEMPERATURE_MAX);
            hprintf128(request,"<form action=\"index\" id=\"form%i\">",SPECIAL_CHANNEL_TEMPERATURE);
            hprintf128(request,"<input type=\"range\" min=\"%i\" max=\"%i\"", HASS_TEMPERATURE_MIN,HASS_TEMPERATURE_MAX);
			hprintf128(request,"name=\"%s\" id=\"slider%i\" value=\"%i\" onchange=\"this.form.submit()\">",inputName,SPECIAL_CHANNEL_TEMPERATURE,pwmValue);
            hprintf128(request,"<input type=\"hidden\" name=\"%sIndex\" value=\"%i\">",inputName,SPECIAL_CHANNEL_TEMPERATURE);
            hprintf128(request,"<input  type=\"submit\" style=\"display:none;\" value=\"Toggle %i\"/></form>",SPECIAL_CHANNEL_TEMPERATURE);
			poststr(request, "</td></tr>");
		} 

	}
    poststr(request, "</table>");
#ifndef OBK_DISABLE_ALL_DRIVERS
	DRV_AppendInformationToHTTPIndexPage(request);
#endif

	if(1) {
		int bFirst = true;
		hprintf128(request,"<h5>");
		for(i = 0; i < CHANNEL_MAX; i++) {
			if(CHANNEL_IsInUse(i)) {
				int value = CHANNEL_Get(i);
				if(bFirst == false) {
					hprintf128(request,", ");
				}
				hprintf128(request,"Channel %i = %i",i,value);
				bFirst = false;
			}
		}
		hprintf128(request,"</h5>");
	}
	hprintf128(request,"<h5>Cfg size: %i, change counter: %i, ota counter: %i, boot incompletes %i (might change to 0 if you wait to 30 sec)!</h5>",
		sizeof(g_cfg),g_cfg.changeCounter,g_cfg.otaCounter,Main_GetLastRebootBootFailures());

	hprintf128(request,"<h5>Ping watchdog - %i lost, %i ok!</h5>",
		PingWatchDog_GetTotalLost(),PingWatchDog_GetTotalReceived());

    // for normal page loads, show the rest of the HTML
    if(!http_getArg(request->url,"state",tmpA,sizeof(tmpA))) {
        poststr(request, "</div>"); // end div#state

        // Shared UI elements 
        poststr(request, "<form action=\"cfg\"><input type=\"submit\" value=\"Config\"/></form>");

        poststr(request, "<form action=\"/index\">"
                "<input type=\"hidden\" id=\"restart\" name=\"restart\" value=\"1\">"
                "<input class=\"bred\" type=\"submit\" value=\"Restart\" onclick=\"return confirm('Are you sure to restart module?')\">"
            "</form>");

        poststr(request, "<form action=\"/app\" target=\"_blank\"><input type=\"submit\" value=\"Launch Web Application\"\"></form> ");
        poststr(request, "<form action=\"about\"><input type=\"submit\" value=\"About\"/></form>");

        poststr(request, htmlFooterRefreshLink);
        http_html_end(request);

        // refresh status section every 3 seconds
        poststr(
            request,
            "<script type='text/javascript'>"
            "var firstTime, lastTime, req=null;" 
            "eb=s=>document.getElementById(s);"
            "function showState() { "
                "clearTimeout(firstTime);"
                "clearTimeout(lastTime);"
                "if (req!=null) { req.abort() }"
                "req=new XMLHttpRequest();"
                "req.onreadystatechange=()=>{"
                    "if(req.readyState==4 && req.status==200){"
                        "if (!(document.activeElement.tagName=='INPUT' && "
                            "(document.activeElement.type=='number' || document.activeElement.type=='color'))) {"
                                "var s=req.responseText;"
                                "eb('state').innerHTML=s;" 
                        "}"
                        "clearTimeout(firstTime);"
                        "clearTimeout(lastTime);"
                        "lastTime=setTimeout(showState, 3e3);"
                "}};"
                "req.open('GET','index?state=1', true);"
                "req.send();"
                "firstTime=setTimeout(showState, 3e3);"
            "}"
            "window.addEventListener('load', showState);"
            "history.pushState(null, '', 'index');" // drop actions like 'toggle' from URL
            "setTimeout(()=>{eb('changed').innerHTML=''}, 5e3);" // hide change info
            "</script>"
        );
    }

	poststr(request, NULL);
    return 0;
}

int http_fn_about(http_request_t *request){
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "About");
    poststr(request,"<h2>Open source firmware for BK7231N, BK7231T, XR809 and BL602 by OpenSHWProjects</h2>");
    poststr(request,htmlFooterReturnToMenu);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}




int http_fn_cfg_mqtt(http_request_t *request) {
    int i;
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "MQTT");
    poststr(request,"<h2> Use this to connect to your MQTT</h2>");
    poststr(request,"<form action=\"/cfg_mqtt_set\">\
            <label for=\"host\">Host:</label><br>\
            <input type=\"text\" id=\"host\" name=\"host\" value=\"");

    poststr(request,CFG_GetMQTTHost());
    poststr(request,"\"><br>\
            <label for=\"port\">Port:</label><br>\
            <input type=\"text\" id=\"port\" name=\"port\" value=\"");
    i = CFG_GetMQTTPort();
    hprintf128(request, "%i", i);
    poststr(request,"\"><br><br>\
            <label for=\"port\">Client ID:</label><br>\
            <input type=\"text\" id=\"client\" name=\"client\" value=\"");

    poststr(request,CFG_GetMQTTClientId());
    poststr(request,"\"><br>\
            <label for=\"user\">User:</label><br>\
            <input type=\"text\" id=\"user\" name=\"user\" value=\"");
    poststr(request,CFG_GetMQTTUserName());
    poststr(request,"\"><br>\
            <label for=\"port\">Password:</label><br>\
            <input type=\"text\" id=\"password\" name=\"password\" value=\"");
    poststr(request,CFG_GetMQTTPass());
    poststr(request,"\"><br>\
            <input type=\"submit\" value=\"Submit\" onclick=\"return confirm('Are you sure? Please check MQTT data twice?')\">\
        </form> ");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_mqtt_set(http_request_t *request) {
	char tmpA[128];
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Saving MQTT");

    if(http_getArg(request->url,"host",tmpA,sizeof(tmpA))) {
        CFG_SetMQTTHost(tmpA);
    }
    if(http_getArg(request->url,"port",tmpA,sizeof(tmpA))) {
        CFG_SetMQTTPort(atoi(tmpA));
    }
    if(http_getArg(request->url,"user",tmpA,sizeof(tmpA))) {
        CFG_SetMQTTUserName(tmpA);
    }
    if(http_getArg(request->url,"password",tmpA,sizeof(tmpA))) {
        CFG_SetMQTTPass(tmpA);
    }
    if(http_getArg(request->url,"client",tmpA,sizeof(tmpA))) {
        CFG_SetMQTTClientId(tmpA);
    }

	CFG_Save_SetupTimer();

    poststr(request,"Please wait for module to connect... if there is problem, restart it from Index html page...");

    poststr(request,"<br>");
    poststr(request,"<a href=\"cfg_mqtt\">Return to MQTT settings</a>");
    poststr(request,"<br>");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_webapp(http_request_t *request) {
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set Webapp");
    poststr(request,"<h2> Use this to set the URL of the Webapp</h2>");
    poststr(request,"<form action=\"/cfg_webapp_set\">\
            <label for=\"url\">Url:</label><br>\
            <input type=\"text\" id=\"url\" name=\"url\" value=\"");
    poststr(request,CFG_GetWebappRoot());
    poststr(request,"\"><br>\
            <input type=\"submit\" value=\"Submit\">\
        </form> ");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_webapp_set(http_request_t *request) {
	char tmpA[128];
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Saving Webapp");

    if(http_getArg(request->url,"url",tmpA,sizeof(tmpA))) {
        if(CFG_SetWebappRoot(tmpA)) {
            hprintf128(request,"Webapp url set to %s", tmpA);
        } else {
            hprintf128(request,"Webapp url change error - failed to save to flash.");
        }
    } else {
        poststr(request,"Webapp url not set because you didn't specify the argument.");
    }

    poststr(request,"<br>");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}





int http_fn_cfg_ping(http_request_t *request) {
	char tmpA[128];
	const char *tmp;
	int i;
	int bChanged;

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set Watchdog");
    bChanged = 0;
    poststr(request,"<h3> Ping watchdog (backup reconnect mechanism)</h3>");
    poststr(request,"<p> By default, all OpenBeken devices automatically tries to reconnect to WiFi when a connection is lost.");
    poststr(request," I have tested the reconnect mechanism many times by restarting my router and it always worked reliably.");
    poststr(request," However, according to some reports, there are still some edge cases when a device fails to reconnect to WIFi.");
    poststr(request," This is why <b>this mechanism</b> has been added.</p>");
    poststr(request,"<p> This mechanism keeps pinging certain host and reconnects to WiFi if it doesn't respond at all for a certain amount of seconds.</p>");
	poststr(request,"<p> USAGE: For a host, choose the main address of your router and make sure it responds to a pings. Interval is 1 second or so, timeout can be set by user, to eg. 60 sec</p>");
    if(http_getArg(request->url,"host",tmpA,sizeof(tmpA))) {
		CFG_SetPingHost(tmpA);
        poststr(request,"<h4> New ping host set!</h4>");
        bChanged = 1;
    }
   /* if(http_getArg(request->url,"interval",tmpA,sizeof(tmpA))) {
		CFG_SetPingIntervalSeconds(atoi(tmpA));
        poststr(request,"<h4> New ping interval set!</h4>");
        bChanged = 1;
    }*/
    if(http_getArg(request->url,"disconnectTime",tmpA,sizeof(tmpA))) {
		CFG_SetPingDisconnectedSecondsToRestart(atoi(tmpA));
        poststr(request,"<h4> New ping disconnectTime set!</h4>");
        bChanged = 1;
    }
    if(http_getArg(request->url,"clear",tmpA,sizeof(tmpA))) {
		CFG_SetPingDisconnectedSecondsToRestart(0);
		CFG_SetPingIntervalSeconds(0);
		CFG_SetPingHost("");
        poststr(request,"<h4> Ping watchdog disabled!</h4>");
        bChanged = 1;
    }
    if(bChanged) {
        poststr(request,"<h4> Changes will be applied after restarting</h4>");
    }
    poststr(request,"<form action=\"/cfg_ping\">\
            <input type=\"hidden\" id=\"clear\" name=\"clear\" value=\"1\">\
            <input type=\"submit\" value=\"Disable ping watchdog!\">\
        </form> ");
    poststr(request,"<h2> Use this to enable pinger</h2>");
    poststr(request,"<form action=\"/cfg_ping\">\
            <label for=\"host\">Host:</label><br>\
            <input type=\"text\" id=\"host\" name=\"host\" value=\"");
	tmp = CFG_GetPingHost();
    poststr(request,tmp);

         /*   poststr(request, "\"><br>\
		<label for=\"pass\">Interval (s) between pings:</label><br>\
            <input type=\"number\" id=\"interval\" name=\"interval\" value=\"");
    i = CFG_GetPingIntervalSeconds();
    hprintf128(request,"%i",i);*/

            poststr(request, "\"><br>\
		<label for=\"pass\">Take action after this number of seconds with no reply:</label><br>\
            <input type=\"number\" id=\"disconnectTime\" name=\"disconnectTime\" value=\"");
			i = CFG_GetPingDisconnectedSecondsToRestart();
    hprintf128(request,"%i",i);

    poststr(request,"\"><br><br>\
            <input type=\"submit\" value=\"Submit\" onclick=\"return confirm('Are you sure?')\">\
        </form> ");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}
int http_fn_cfg_wifi(http_request_t *request) {
    // for a test, show password as well...
    const char *cur_ssid, *cur_pass;
	char tmpA[128];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set Wifi");
    /*bChanged = 0;
    if(http_getArg(recvbuf,"ssid",tmpA,sizeof(tmpA))) {
        CFG_SetWiFiSSID(tmpA);
        poststr(request,"<h4> WiFi SSID set!</h4>");
        bChanged = 1;
    }
    if(http_getArg(recvbuf,"pass",tmpA,sizeof(tmpA))) {
        CFG_SetWiFiPass(tmpA);
        poststr(request,"<h4> WiFi Password set!</h4>");
        bChanged = 1;
    }
    if(bChanged) {
        poststr(request,"<h4> Device will reconnect after restarting</h4>");
    }*/
    poststr(request,"<h2> Check networks reachable by module</h2> This will lag few seconds.<br>");
    if(http_getArg(request->url,"scan",tmpA,sizeof(tmpA))) {
#ifdef WINDOWS

        poststr(request,"Not available on Windows<br>");
#elif PLATFORM_XR809
        poststr(request,"TODO XR809<br>");

#elif PLATFORM_W800
        poststr(request,"TODO W800<br>");
#elif PLATFORM_BL602
        poststr(request,"TODO BL602<br>");
#elif PLATFORM_BK7231T
		int i;

        AP_IF_S *ar;
        uint32_t num;

        bk_printf("Scan begin...\r\n");
        tuya_hal_wifi_all_ap_scan(&ar,&num);
        bk_printf("Scan returned %i networks\r\n",num);
        for(i = 0; i < num; i++) {
            hprintf128(request,"[%i/%i] SSID: %s, Channel: %i, Signal %i<br>",i,(int)num,ar[i].ssid, ar[i].channel, ar[i].rssi);
        }
        tuya_hal_wifi_release_ap(ar);
#elif PLATFORM_BK7231N
		int i;

        AP_IF_S *ar;
        uint32_t num;

        bk_printf("Scan begin...\r\n");
        tuya_os_adapt_wifi_all_ap_scan(&ar,&num);
        bk_printf("Scan returned %i networks\r\n",num);
        for(i = 0; i < num; i++) {
            hprintf128(request,"[%i/%i] SSID: %s, Channel: %i, Signal %i<br>",i + 1,(int)num,ar[i].ssid, ar[i].channel, ar[i].rssi);
        }
        tuya_os_adapt_wifi_release_ap(ar);
#else
#error "Unknown platform"
        poststr(request,"Unknown platform<br>");
#endif
    }
    poststr(request,"<form action=\"/cfg_wifi\">\
            <input type=\"hidden\" id=\"scan\" name=\"scan\" value=\"1\">\
            <input type=\"submit\" value=\"Scan local networks!\">\
        </form> ");
    poststr(request,"<h2> Use this to disconnect from your WiFi</h2>");
    poststr(request,"<form action=\"/cfg_wifi_set\">\
            <input type=\"hidden\" id=\"open\" name=\"open\" value=\"1\">\
            <input type=\"submit\" value=\"Convert to open access wifi\" onclick=\"return confirm('Are you sure to convert module to open access wifi?')\">\
        </form> ");
    poststr(request,"<h2> Use this to connect to your WiFi</h2>");
    poststr(request,"<form action=\"/cfg_wifi_set\">\
            <label for=\"ssid\">SSID:</label><br>\
            <input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"");
    cur_ssid = CFG_GetWiFiSSID();
    poststr(request,cur_ssid);

            poststr(request, "\"><br>\
            <label for=\"pass\">Pass:</label><br>\
            <input type=\"text\" id=\"pass\" name=\"pass\" value=\"");
    cur_pass = CFG_GetWiFiPass();
    poststr(request,cur_pass);

    poststr(request,"\"><br><br>\
            <input type=\"submit\" value=\"Submit\" onclick=\"return confirm('Are you sure? Please check SSID and pass twice?')\">\
        </form> ");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_name(http_request_t *request) {
    // for a test, show password as well...
    const char *shortName, *name;
	char tmpA[128];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set name");

    poststr(request,"<h2> Change device names for display. </h2>");
    if(http_getArg(request->url,"shortName",tmpA,sizeof(tmpA))) {
		CFG_SetShortDeviceName(tmpA);
    }
    if(http_getArg(request->url,"name",tmpA,sizeof(tmpA))) {
		CFG_SetDeviceName(tmpA);
    }
    poststr(request,"<h2> Use this to change device names</h2>");
    poststr(request,"<form action=\"/cfg_name\">\
            <label for=\"shortName\">ShortName:</label><br>\
            <input type=\"text\" id=\"shortName\" name=\"shortName\" value=\"");
	shortName = CFG_GetShortDeviceName();
    poststr(request,shortName);

            poststr(request, "\"><br>\
            <label for=\"name\">Full Name:</label><br>\
            <input type=\"text\" id=\"name\" name=\"name\" value=\"");
			name = CFG_GetDeviceName();
    poststr(request,name);

    poststr(request,"\"><br><br>");
    poststr(request, "<input type=\"submit\" value=\"Submit\" "
            "onclick=\"return confirm('Are you sure? "
            "Short name might be used by Home Assistant, "
            "so you will have to reconfig some stuff.')\">");
    poststr(request, "</form>");
    //poststr(request,htmlReturnToCfg);
    //HTTP_AddBuildFooter(request);
    //poststr(request,htmlEnd);
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_wifi_set(http_request_t *request) {
	char tmpA[128];
	addLogAdv(LOG_INFO, LOG_FEATURE_HTTP,"HTTP_ProcessPacket: generating cfg_wifi_set \r\n");

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Saving Wifi");
    if(http_getArg(request->url,"open",tmpA,sizeof(tmpA))) {
        CFG_SetWiFiSSID("");
        CFG_SetWiFiPass("");
        poststr(request,"WiFi mode set: open access point.");
    } else {
        if(http_getArg(request->url,"ssid",tmpA,sizeof(tmpA))) {
            CFG_SetWiFiSSID(tmpA);
        }
        if(http_getArg(request->url,"pass",tmpA,sizeof(tmpA))) {
            CFG_SetWiFiPass(tmpA);
        }
        poststr(request,"WiFi mode set: connect to WLAN.");
    }
	CFG_Save_SetupTimer();

    poststr(request,"Please wait for module to reset...");
	RESET_ScheduleModuleReset(3);

    poststr(request,"<br>");
    poststr(request,"<a href=\"cfg_wifi\">Return to WiFi settings</a>");
    poststr(request,"<br>");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_loglevel_set(http_request_t *request) {
	char tmpA[128];
    addLogAdv(LOG_INFO, LOG_FEATURE_HTTP,"HTTP_ProcessPacket: generating cfg_loglevel_set \r\n");

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set log level");
    if(http_getArg(request->url,"loglevel",tmpA,sizeof(tmpA))) {
#if WINDOWS
#else
        loglevel = atoi(tmpA);
#endif
        poststr(request,"LOG level changed.");
    }
    poststr(request,"<form action=\"/cfg_loglevel_set\">\
            <label for=\"loglevel\">loglevel:</label><br>\
            <input type=\"text\" id=\"loglevel\" name=\"loglevel\" value=\"");
    tmpA[0] = 0;
#if WINDOWS
#else
    hprintf128(request,"%i",loglevel);
#endif
    poststr(request,"\"><br><br>\
            <input type=\"submit\" value=\"Submit\" >\
        </form> ");

    poststr(request,"<br>");
    poststr(request,"<a href=\"cfg\">Return to config settings</a>");
    poststr(request,"<br>");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}


int http_fn_cfg_mac(http_request_t *request) {
    // must be unsigned, else print below prints negatives as e.g. FFFFFFFe
    unsigned char mac[6];
	char tmpA[128];
    int i;

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set MAC address");

    if(http_getArg(request->url,"mac",tmpA,sizeof(tmpA))) {
        for( i = 0; i < 6; i++ )
        {
            mac[i] = hexbyte( &tmpA[i * 2] ) ;
        }
        //sscanf(tmpA,"%02X%02X%02X%02X%02X%02X",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
        if(WiFI_SetMacAddress((char*)mac)) {
            poststr(request,"<h4> New MAC set!</h4>");
        } else {
            poststr(request,"<h4> MAC change error?</h4>");
        }
		CFG_Save_IfThereArePendingChanges();
    }

    WiFI_GetMacAddress((char *)mac);

    poststr(request,"<h2> Here you can change MAC address.</h2>");
    poststr(request,"<form action=\"/cfg_mac\">\
            <label for=\"mac\">MAC:</label><br>\
            <input type=\"text\" id=\"mac\" name=\"mac\" value=\"");
    hprintf128(request,"%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    poststr(request,"\"><br><br>\
            <input type=\"submit\" value=\"Submit\" onclick=\"return confirm('Are you sure? Please check MAC hex string twice?')\">\
        </form> ");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_flash_read_tool(http_request_t *request) {
    int len = 16;
    int ofs = 1970176;
    int res;
    int rem;
    int now;
    int nowOfs;
    int hex;
    int i;
	char tmpA[128];
	char tmpB[64];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Flash read");
    poststr(request,"<h4>Flash Read Tool</h4>");
    if(	http_getArg(request->url,"hex",tmpA,sizeof(tmpA))){
        hex = atoi(tmpA);
    } else {
        hex = 0;
    }

    if(	http_getArg(request->url,"offset",tmpA,sizeof(tmpA)) &&
            http_getArg(request->url,"len",tmpB,sizeof(tmpB))) {
        unsigned char buffer[128];
        len = atoi(tmpB);
        ofs = atoi(tmpA);
        hprintf128(request,"Memory at %i with len %i reads: ",ofs,len);
        poststr(request,"<br>");

        ///res = bekken_hal_flash_read (ofs, buffer,len);
        //sprintf(tmpA,"Result %i",res);
    //	strcat(outbuf,tmpA);
    ///	strcat(outbuf,"<br>");

        nowOfs = ofs;
        rem = len;
        while(1) {
            if(rem > sizeof(buffer)) {
                now = sizeof(buffer);
            } else {
                now = rem;
            }
#if PLATFORM_XR809
            //uint32_t flash_read(uint32_t flash, uint32_t addr,void *buf, uint32_t size)
            #define FLASH_INDEX_XR809 0
            res = flash_read(FLASH_INDEX_XR809, nowOfs, buffer, now);
#elif PLATFORM_BL602

#elif PLATFORM_W800

#else
            res = bekken_hal_flash_read (nowOfs, buffer,now);
#endif
            for(i = 0; i < now; i++) {
                unsigned char val = buffer[i];
                if(!hex && isprint(val)) {
                    hprintf128(request,"'%c' ",val);
                } else {
                    hprintf128(request,"%02X ",val);
                }
            }
            rem -= now;
            nowOfs += now;
            if(rem <= 0) {
                break;
            }
        }

        poststr(request,"<br>");
    }
    poststr(request,"<form action=\"/flash_read_tool\">");

    poststr(request,"<input type=\"checkbox\" id=\"hex\" name=\"hex\" value=\"1\"");
    if(hex){
        poststr(request," checked");
    }
    poststr(request,"><label for=\"hex\">Show all hex?</label><br>");
    poststr(request,"<label for=\"offset\">offset:</label><br>\
            <input type=\"number\" id=\"offset\" name=\"offset\"");
    hprintf128(request," value=\"%i\"><br>",ofs);
    poststr(request,"<label for=\"len\">length:</label><br>\
            <input type=\"number\" id=\"len\" name=\"len\" ");
    hprintf128(request,"value=\"%i\">",len);
    poststr(request,"<br><br>\
            <input type=\"submit\" value=\"Submit\">\
        </form> ");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cmd_tool(http_request_t *request) {
    int i;
	char tmpA[128];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Command tool");
    poststr(request,"<h4>Command Tool</h4>");

    if(	http_getArg(request->url,"cmd",tmpA,sizeof(tmpA))) {
		i = CMD_ExecuteCommand(tmpA, COMMAND_FLAG_SOURCE_CONSOLE);
		if(i == 0) {
		   poststr(request,"Command not found");
		} else {
		   poststr(request,"Executed");
		}
        poststr(request,"<br>");
    }
    poststr(request,"<form action=\"/cmd_tool\">");

    poststr(request,"<label for=\"cmd\">cmd:</label><br>\
            <input type=\"text\" id=\"cmd\" name=\"cmd\" ");
    hprintf128(request,"value=\"%s\"  size=\"80\">",tmpA);
    poststr(request,"<br><br>\
            <input type=\"submit\" value=\"Submit\">\
        </form> ");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_startup_command(http_request_t *request) {
	char tmpA[512];
	const char *cmd;
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Set startup command");
    poststr(request,"<h4>Set/Change/Clear startup command line</h4>");
    poststr(request,"<h5>Startup command is a shorter, smaller alternative to LittleFS autoexec.bat."
		"The startup commands are ran at device startup."
		"You can use them to init peripherals and drivers, like BL0942 energy sensor</h5>");

    if(http_getArg(request->url,"data",tmpA,sizeof(tmpA))) {
      //  hprintf128(request,"<h3>Set command to  %s!</h3>",tmpA);
		// tmpA can be longer than 128 bytes and this would crash
        hprintf128(request,"<h3>Command changed!</h3>");
		CFG_SetShortStartupCommand(tmpA);

		CFG_Save_IfThereArePendingChanges();
	} else {
	}

	cmd = CFG_GetShortStartupCommand();

    poststr(request,"<form action=\"/startup_command\">");

    poststr(request,"<label for=\"data\">Startup command:</label><br>\
            <input type=\"text\" id=\"data\" name=\"data\"");
    hprintf128(request," value=\"");
    poststr(request,cmd);
    hprintf128(request,"\"  size=\"120\"><br>");
    poststr(request,"<br>\
            <input type=\"submit\" value=\"Submit\">\
        </form> ");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}
int http_fn_uart_tool(http_request_t *request) {
	char tmpA[256];
	int resultLen = 0;
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "UART tool");
    poststr(request,"<h4>UART Tool</h4>");

    if(http_getArg(request->url,"data",tmpA,sizeof(tmpA))) {
#ifndef OBK_DISABLE_ALL_DRIVERS
		byte results[128];

        hprintf128(request,"<h3>Sent %s!</h3>",tmpA);
		if(0){
			TuyaMCU_Send((byte *)tmpA, strlen(tmpA));
		//	bk_send_string(0,tmpA);
		} else {
			byte b;
			const char *p;

			p = tmpA;
			while(*p) {
				b = hexbyte(p);
				results[resultLen] = b;
				resultLen++;
				p+=2;
			}
			TuyaMCU_Send(results, resultLen);
		}
#endif
	} else {
		strcpy(tmpA,"Hello UART world");
	}

    poststr(request,"<form action=\"/uart_tool\">");

    poststr(request,"<label for=\"data\">data:</label><br>\
            <input type=\"text\" id=\"data\" name=\"data\"");
    hprintf128(request," value=\"%s\"  size=\"40\"><br>",tmpA);
    poststr(request,"<br>\
            <input type=\"submit\" value=\"Submit\">\
        </form> ");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_config_dump_table(http_request_t *request) {
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Dump config");
    poststr(request,"Not implemented <br>");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}




int http_fn_cfg_quick(http_request_t *request) {
	char tmpA[128];
    int j;
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Quick Config");
    poststr(request,"<h4>Quick Config</h4>");

    if(http_getArg(request->url,"dev",tmpA,sizeof(tmpA))) {
        j = atoi(tmpA);
        hprintf128(request,"<h3>Set dev %i!</h3>",j);
        g_templates[j].setter();
    }
    poststr(request,"<form action=\"cfg_quick\">");
    poststr(request, "<select name=\"dev\">");
    for(j = 0; j < g_total_templates; j++) {
        hprintf128(request, "<option value=\"%i\">%s</option>",j,g_templates[j].name);
    }
    poststr(request,"</select>");
    poststr(request,"<input type=\"submit\" value=\"Set\"/></form>");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

/// @brief Computes the Relay and PWM count.
/// @param relayCount 
/// @param pwmCount 
void get_Relay_PWM_Count(int *relayCount, int *pwmCount){
    (*relayCount) = 0;
    (*pwmCount) = 0;

    for(int i = 0; i < PLATFORM_GPIO_MAX; i++) {
        int role = PIN_GetPinRoleForPinIndex(i);
        if(role == IOR_Relay || role == IOR_Relay_n || role == IOR_LED || role == IOR_LED_n) {
            (*relayCount)++;
        }
        else if(role == IOR_PWM || role == IOR_PWM_n) {
            (*pwmCount)++;
        }
    }
}

/// @brief Sends HomeAssistant discovery MQTT messages.
/// @param request 
/// @return 
int http_fn_ha_discovery(http_request_t *request) {
    int i;
    char topic[32];
    int relayCount=0;
    int pwmCount=0;

    http_setup(request, httpMimeTypeText);
    get_Relay_PWM_Count(&relayCount, &pwmCount);

    if ((relayCount == 0) && (pwmCount == 0)) {
        poststr(request, NULL);
        return 0;
    }

    if (!http_getArg(request->url, "prefix", topic, sizeof(topic))) {
        sprintf(topic, "homeassistant");    //default discovery topic is `homeassistant`
    }
    
    cJSON_Hooks hooks = {os_malloc, os_free};
    cJSON_InitHooks(&hooks);
    
    if(relayCount > 0) {
        for(i = 0; i < CHANNEL_MAX; i++) {
            if(h_isChannelRelay(i)) {
                HassDeviceInfo *dev_info = hass_init_device_info(ENTITY_RELAY, i, "1", "0");
                MQTT_QueuePublish(topic, dev_info->channel, hass_build_discovery_json(dev_info), OBK_PUBLISH_FLAG_RETAIN);
                hass_free_device_info(dev_info);
            }
        }
    }

    if(pwmCount > 0) {
        char tmp[64];
        const char *shortDeviceName = CFG_GetShortDeviceName();

        for(i = 0; i < CHANNEL_MAX; i++) {
            if(h_isChannelPWM(i)) {
                HassDeviceInfo *dev_info = hass_init_device_info(ENTITY_LIGHT, i, "99", "0");

                cJSON_AddStringToObject(dev_info->root, "on_cmd_type", "brightness"); //on_command_type
                cJSON_AddNumberToObject(dev_info->root, "bri_scl", 99);   //brightness_scale

                sprintf(tmp,"%s/%i/set",shortDeviceName,i);
                cJSON_AddStringToObject(dev_info->root, "bri_cmd_t", tmp);    //brightness_command_topic

                MQTT_QueuePublish(topic, dev_info->channel, hass_build_discovery_json(dev_info), OBK_PUBLISH_FLAG_RETAIN);
                hass_free_device_info(dev_info);
            }
        }
    }
    
    poststr(request, NULL);
    return 0;
}

int http_fn_ha_cfg(http_request_t *request) {
    int relayCount = 0;
    int pwmCount = 0;
    const char *shortDeviceName;
    const char *clientId;
    int i;
    char mqttAdded = 0;
    char *uniq_id;

    shortDeviceName = CFG_GetShortDeviceName();
    clientId = CFG_GetMQTTClientId();

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Home Assistant Setup");
    poststr(request,"<h4>Home Assistant Cfg</h4>");
	hprintf128(request,"<h4>Note that your short device name is: %s</h4>",shortDeviceName);
    poststr(request,"<h4>Paste this to configuration yaml</h4>");
    poststr(request,"<h5>Make sure that you have \"switch:\" keyword only once! Home Assistant doesn't like dup keywords.</h5>");
    poststr(request,"<h5>You can also use \"switch MyDeviceName:\" to avoid keyword duplication!</h5>");

    poststr(request,"<textarea rows=\"40\" cols=\"50\">");

    get_Relay_PWM_Count(&relayCount, &pwmCount);
    
    if(relayCount > 0) {
        char switchAdded = 0;

        for(i = 0; i < CHANNEL_MAX; i++) {
            if(h_isChannelRelay(i)) {
                if (mqttAdded == 0){
                    poststr(request,"mqtt:\n");
                    mqttAdded=1;
                }
                if (switchAdded == 0){
                    poststr(request,"  switch:\n");
                    switchAdded=1;
                }

                uniq_id = hass_build_unique_id(ENTITY_RELAY,i);
                hprintf128(request,"  - unique_id: \"%s\"\n",uniq_id);
                os_free(uniq_id);

                hprintf128(request,"    name: \"%s %i\"\n",shortDeviceName,i);
                hprintf128(request,"    state_topic: \"%s/%i/get\"\n",clientId,i);
                hprintf128(request,"    command_topic: \"%s/%i/set\"\n",clientId,i);
                poststr(request,   "    qos: 1\n");
                poststr(request,   "    payload_on: 1\n");
                poststr(request,   "    payload_off: 0\n");
                poststr(request,   "    retain: true\n");
                hprintf128(request,"    availability:\n");
                hprintf128(request,"      - topic: \"%s/connected\"\n",clientId);
            }
        }
    }
    if(pwmCount > 0) {
        char lightAdded = 0;
                
        for(i = 0; i < CHANNEL_MAX; i++) {
            if(h_isChannelPWM(i)) {
                if (mqttAdded == 0){
                    poststr(request,"mqtt:\n");
                    mqttAdded=1;
                }
                if (lightAdded == 0){
                    poststr(request,"  light:\n");
                    lightAdded=1;
                }

                uniq_id = hass_build_unique_id(ENTITY_LIGHT,i);
                hprintf128(request,"  - unique_id: \"%s\"\n",uniq_id);
                os_free(uniq_id);

                hprintf128(request,"    name: \"%s %i\"\n",shortDeviceName,i);
                hprintf128(request,"    state_topic: \"%s/%i/get\"\n",clientId,i);
                hprintf128(request,"    command_topic: \"%s/%i/set\"\n",clientId,i);
                hprintf128(request,"    brightness_command_topic: \"%s/%i/set\"\n",clientId,i);
                poststr(request,   "    on_command_type: \"brightness\"\n");
                poststr(request,   "    brightness_scale: 99\n");
                poststr(request,   "    qos: 1\n");
                poststr(request,   "    payload_on: 99\n");
                poststr(request,   "    payload_off: 0\n");
                poststr(request,   "    retain: true\n");
                poststr(request,   "    optimistic: true\n");
                hprintf128(request,"    availability:\n");
                hprintf128(request,"      - topic: \"%s/connected\"\n",clientId);
            }
        }
    }

    poststr(request,"</textarea>");
    poststr(request,"<br/><div><label for=\"ha_disc_topic\">Discovery topic:</label><input id=\"ha_disc_topic\" value=\"homeassistant\"><button onclick=\"send_ha_disc();\">Start Home Assistant Discovery</button>&nbsp;<form action=\"cfg_mqtt\" style=\"display:inline-block;\"><button type=\"submit\">Configure MQTT</button></form></div><br/>");
    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
    poststr(request, HomeAssistantDiscoveryScript);
	poststr(request, NULL);
    return 0;
}

// https://tasmota.github.io/docs/Commands/#with-mqtt
/*
http://<ip>/cm?cmnd=Power%20TOGGLE
http://<ip>/cm?cmnd=Power%20On
http://<ip>/cm?cmnd=Power%20off
http://<ip>/cm?user=admin&password=joker&cmnd=Power%20Toggle
*/
// https://www.elektroda.com/rtvforum/viewtopic.php?p=19330027#19330027
// Web browser sends: GET /cm?cmnd=POWER1
// System responds with state
int http_tasmota_json_power(http_request_t *request) {
	int numRelays;
	int numPWMs;
	int i;
	int lastRelayState;

	// try to return status
	numPWMs = PIN_CountPinsWithRoleOrRole(IOR_PWM, IOR_PWM_n);
	numRelays = 0;

	// LED driver (if has PWMs)
	if(numPWMs > 0){
		if(LED_GetEnableAll() == 0) {
			poststr(request,"{\"POWER\":\"OFF\"}");
		} else {
			poststr(request,"{\"POWER\":\"ON\"}");
		}
	} else {
		// relays driver
		for(i = 0; i < CHANNEL_MAX; i++) {
			if (h_isChannelRelay(i) || CHANNEL_GetType(i) == ChType_Toggle) {
				numRelays++;
				lastRelayState = CHANNEL_Get(i);
			}
		}
		if(numRelays == 1) {
			if(lastRelayState) {
				poststr(request,"{\"POWER\":\"ON\"}");
			} else {
				poststr(request,"{\"POWER\":\"OFF\"}");
			}
		} else {
			for(i = 0; i < CHANNEL_MAX; i++) {
				if (h_isChannelRelay(i) || CHANNEL_GetType(i) == ChType_Toggle) {
					lastRelayState = CHANNEL_Get(i);
					if(lastRelayState) {
						hprintf128(request,"{\"POWER%i\":\"ON\"}",i);
					} else {
						hprintf128(request,"{\"POWER%i\":\"OFF\"}",i);
					}
				}
			}
		}

	}
	return 0;
}
/*
{"StatusSNS":{"Time":"2022-07-30T10:11:26","ENERGY":{"TotalStartTime":"2022-05-12T10:56:31","Total":0.003,"Yesterday":0.003,"Today":0.000,"Power": 0,"ApparentPower": 0,"ReactivePower": 0,"Factor":0.00,"Voltage":236,"Current":0.000}}}
*/
int http_tasmota_json_status_SNS(http_request_t *request) {
	float power, factor, voltage, current;

#ifndef OBK_DISABLE_ALL_DRIVERS
	factor = 0; // TODO
	voltage = DRV_GetReading(OBK_VOLTAGE);
	current = DRV_GetReading(OBK_CURRENT);
	power = DRV_GetReading(OBK_POWER);
#else
	factor = 0; 
	voltage = 0;
	current = 0;
	power = 0;
#endif

	hprintf128(request,"{\"StatusSNS\":{\"ENERGY\":{");
	hprintf128(request,"\"Power\": %f,", power);
	hprintf128(request,"\"ApparentPower\": 0,\"ReactivePower\": 0,\"Factor\":%f,", factor);
	hprintf128(request,"\"Voltage\":%f,", voltage);
	hprintf128(request,"\"Current\":%f}}}", current);

	return 0;
}
/*
{"Status":{"Module":0,"DeviceName":"Tasmota","FriendlyName":["Tasmota"],"Topic":"tasmota_48E7F3","ButtonTopic":"0","Power":1,"PowerOnState":3,"LedState":1,"LedMask":"FFFF","SaveData":1,"SaveState":1,"SwitchTopic":"0","SwitchMode":[0,0,0,0,0,0,0,0],"ButtonRetain":0,"SwitchRetain":0,"SensorRetain":0,"PowerRetain":0,"InfoRetain":0,"StateRetain":0}}
*/
int http_tasmota_json_status_generic(http_request_t *request) {
	const char *deviceName;
	const char *friendlyName;
	const char *clientId;

	deviceName = CFG_GetShortDeviceName();
	friendlyName = CFG_GetDeviceName();
	clientId = CFG_GetMQTTClientId();

	hprintf128(request,"{\"Status\":{\"Module\":0,\"DeviceName\":\"%s\"",deviceName);
	hprintf128(request,",\"FriendlyName\":[\"%s\"]",friendlyName);
	hprintf128(request,",\"Topic\":\"%s\",\"ButtonTopic\":\"0\"",clientId);
	hprintf128(request,",\"Power\":1,\"PowerOnState\":3,\"LedState\":1");
	hprintf128(request,",\"LedMask\":\"FFFF\",\"SaveData\":1,\"SaveState\":1");
	hprintf128(request,",\"SwitchTopic\":\"0\",\"SwitchMode\":[0,0,0,0,0,0,0,0]");
	hprintf128(request,",\"ButtonRetain\":0,\"SwitchRetain\":0,\"SensorRetain\":0");
	hprintf128(request,",\"PowerRetain\":0,\"InfoRetain\":0,\"StateRetain\":0}}");



	return 0;
}
int http_fn_cm(http_request_t *request) {
	char tmpA[128];

    http_setup(request, httpMimeTypeJson);
	// exec command
    if(	http_getArg(request->url,"cmnd",tmpA,sizeof(tmpA))) {
		CMD_ExecuteCommand(tmpA,COMMAND_FLAG_SOURCE_HTTP);

		if(!wal_strnicmp(tmpA,"POWER",5)) {
			http_tasmota_json_power(request);
		} else if(!wal_strnicmp(tmpA,"STATUS 8",8)) {
			http_tasmota_json_status_SNS(request);
		} else {
			http_tasmota_json_status_generic(request);
		}

    }

	poststr(request, NULL);


    return 0;
}

int http_fn_cfg(http_request_t *request) {
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Config");
    postFormAction(request, "cfg_pins", "Configure Module");
    postFormAction(request, "cfg_generic", "Configure General");
    postFormAction(request, "cfg_startup", "Configure Startup");
    postFormAction(request, "cfg_dgr", "Configure Device Groups");
    postFormAction(request, "cfg_quick","Quick Config");
    postFormAction(request, "cfg_wifi", "Configure WiFi");
    postFormAction(request, "cfg_mqtt", "Configure MQTT");
    postFormAction(request, "cfg_name", "Configure Names");
    postFormAction(request, "cfg_mac", "Change MAC");
    postFormAction(request, "cfg_ping", "Ping Watchdog (Network lost restarter)");
    postFormAction(request, "cfg_webapp", "Configure Webapp");
    postFormAction(request, "ha_cfg", "Generate Home Assistant cfg");
    postFormAction(request, "ota", "OTA (update software by WiFi)");
    postFormAction(request, "cmd_tool", "Execute custom command");
    postFormAction(request, "flash_read_tool" ,"Flash Read Tool");
    postFormAction(request, "uart_tool", "UART Tool");
    postFormAction(request, "startup_command", "Change startup command text");

#if 0
#if PLATFORM_BK7231T | PLATFORM_BK7231N
	{
		int i,j,k;
		k = config_get_tableOffsets(BK_PARTITION_NET_PARAM,&i,&j);
		hprintf128(request,"BK_PARTITION_NET_PARAM: bOk %i, at %i, len %i<br>",k,i,j);
		k = config_get_tableOffsets(BK_PARTITION_RF_FIRMWARE,&i,&j);
		hprintf128(request,"BK_PARTITION_RF_FIRMWARE: bOk %i, at %i, len %i<br>",k,i,j);
		k = config_get_tableOffsets(BK_PARTITION_OTA,&i,&j);
		hprintf128(request,"BK_PARTITION_OTA: bOk %i, at %i, len %i<br>",k,i,j);
	}
#endif
#endif
    poststr(request,htmlFooterReturnToMenu);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_pins(http_request_t *request) {
    int iChanged = 0;
    int iChangedRequested = 0;
    int i;
	char tmpA[128];
	char tmpB[64];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Pin config");
    poststr(request,"<h5> First textfield is used to enter channel index (relay index), used to support multiple relays and buttons</h5>");
    poststr(request,"<h5> (so, first button and first relay should have channel 1, second button and second relay have channel 2, etc)</h5>");
    poststr(request,"<h5> Second textfield (only for buttons) is used to enter channel to toggle when doing double click</h5>");
    poststr(request,"<h5> (second textfield shows up when you change role to button and save...)</h5>");
#if PLATFORM_BK7231N || PLATFORM_BK7231T
    poststr(request,"<h5>BK7231N/BK7231T supports PWM only on pins 6, 7, 8, 9, 24 and 26!</h5>");
#endif
    for(i = 0; i < PLATFORM_GPIO_MAX; i++) {
        sprintf(tmpA, "%i",i);
        if(http_getArg(request->url,tmpA,tmpB,sizeof(tmpB))) {
            int role;
            int pr;

            iChangedRequested++;

            role = atoi(tmpB);

            pr = PIN_GetPinRoleForPinIndex(i);
            if(pr != role) {
                PIN_SetPinRoleForPinIndex(i,role);
                iChanged++;
            }
        }
        sprintf(tmpA, "r%i",i);
        if(http_getArg(request->url,tmpA,tmpB,sizeof(tmpB))) {
            int rel;
            int prevRel;

            iChangedRequested++;

            rel = atoi(tmpB);

            prevRel = PIN_GetPinChannelForPinIndex(i);
            if(prevRel != rel) {
                PIN_SetPinChannelForPinIndex(i,rel);
                iChanged++;
            }
        }
        sprintf(tmpA, "e%i",i);
        if(http_getArg(request->url,tmpA,tmpB,sizeof(tmpB))) {
            int rel;
            int prevRel;

            iChangedRequested++;

            rel = atoi(tmpB);

            prevRel = PIN_GetPinChannel2ForPinIndex(i);
            if(prevRel != rel) {
                PIN_SetPinChannel2ForPinIndex(i,rel);
                iChanged++;
            }
        }
    }
    if(iChangedRequested>0) {
        // Anecdotally, if pins are configured badly, the
        // second-timer breaks. To reconfigure, force
        // saving the configuration instead of waiting.
		//CFG_Save_SetupTimer(); 
        CFG_Save_IfThereArePendingChanges(); 
        hprintf128(request, "Pins update - %i reqs, %i changed!<br><br>",iChangedRequested,iChanged);
    }
//	strcat(outbuf,"<button type=\"button\">Click Me!</button>");
    poststr(request,"<form action=\"cfg_pins\">");
    for(i = 0; i < PLATFORM_GPIO_MAX; i++) {
        int si, ch, ch2;
        int j;
		const char *alias;
		// On BL602, any GPIO can be mapped to one of 5 PWM channels
		// But on Beken chips, only certain pins can be PWM
		int bCanThisPINbePWM;

        si = PIN_GetPinRoleForPinIndex(i);
        ch = PIN_GetPinChannelForPinIndex(i);
        ch2 = PIN_GetPinChannel2ForPinIndex(i);

		bCanThisPINbePWM = HAL_PIN_CanThisPinBePWM(i);

		// if available..
		alias = HAL_PIN_GetPinNameAlias(i);
        poststr(request, "<div class=\"hdiv\">");
		if(alias) {
			poststr(request,alias);
			poststr(request," ");
		} else {
			hprintf128(request, "P%i ",i);
		}
        hprintf128(request, "<select class=\"hele\" name=\"%i\">",i);
        for(j = 0; j < IOR_Total_Options; j++) {
			// do not show hardware PWM on non-PWM pin
			if(j == IOR_PWM || j == IOR_PWM_n) {
				if(bCanThisPINbePWM == 0) {
					continue;
				}
			}

            if(j == si) {
                hprintf128(request, "<option value=\"%i\" selected>%s</option>",j,htmlPinRoleNames[j]);
            } else {
                hprintf128(request, "<option value=\"%i\">%s</option>",j,htmlPinRoleNames[j]);
            }
        }
        poststr(request, "</select>");
        hprintf128(request, "<input class=\"hele\" name=\"r%i\" type=\"text\" value=\"%i\"/>",i,ch);

		if(si == IOR_Button || si == IOR_Button_n)
		{
			// extra param. For button, is relay index to toggle on double click
			hprintf128(request, "<input class=\"hele\" name=\"e%i\" type=\"text\" value=\"%i\"/>",i,ch2);
		}
        poststr(request,"</div>");
    }
    poststr(request,"<input type=\"submit\" value=\"Save\"/></form>");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}
const char *g_obk_flagNames[] = {
	"[MQTT] Broadcast led params together (send dimmer and color when dimmer or color changes, topic name: YourDevName/led_basecolor_rgb/get, YourDevName/led_dimmer/get)",
	"[MQTT] Broadcast led final color (topic name: YourDevName/led_finalcolor_rgb/get)",
	"[MQTT] Broadcast self state every minute (May cause device disconnect's, DONT USE IT YET)",
	"[LED][Debug] Show raw PWM controller on WWW index instead of new LED RGB/CW/etc picker",
	"[LED] Force show RGBCW controller (for example, for SM2135 LEDs, or for DGR sender)",
	"[CMD] Enable TCP console command server (for Putty, etc)",
	"[BTN] Instant touch reaction instead of waiting for release (aka SetOption 13)",
	"[MQTT] [Debug] Always set Retain flag to all published values",
	"[LED] Alternate CW light mode (first PWM for warm/cold slider, second for brightness)",
	"[SM2135] Use separate RGB/CW modes instead of writing all 5 values as RGB",
	"[MQTT] Broadcast self state on MQTT connect",
	"[PWM] BK7231 use 600hz instead of 1khz default",
	"[LED] remember LED driver state (RGBCW, enable, brightness, temperature) after reboot",
	"error",
	"error",
};

int http_fn_cfg_generic(http_request_t *request) {
    int i;
	char tmpA[64];
	char tmpB[64];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Generic config");

    if(	http_getArg(request->url,"boot_ok_delay",tmpA,sizeof(tmpA))) {
		i = atoi(tmpA);
		if(i <= 0) {
			poststr(request,"<h5>Boot ok delay must be at least 1 second<h5>");
			i = 1;
		}
		hprintf128(request,"<h5>Setting boot OK delay to %i<h5>",i);
		CFG_SetBootOkSeconds(i);
    }

    if(	http_getArg(request->url,"setFlags",tmpA,sizeof(tmpA))) {
		for(i = 0; i < OBK_TOTAL_FLAGS; i++) {
			int ni;
			sprintf(tmpB,"flag%i",i);

			if(	http_getArg(request->url,tmpB,tmpA,sizeof(tmpA))) {
				ni = atoi(tmpA);
			} else {
				ni = 0;
			}
			hprintf128(request,"<h5>Setting flag %i to %i<h5>",i,ni);
			CFG_SetFlag(i,ni);
		}
    }

	CFG_Save_IfThereArePendingChanges();

	hprintf128(request,"<h5>Flags (Current value=%i)<h5>",CFG_GetFlags());
	poststr(request,"<form action=\"/cfg_generic\">");

	for(i = 0; i < OBK_TOTAL_FLAGS; i++) {
		const char *flagName = g_obk_flagNames[i];
/*
<div><input type="checkbox" name="flag0" id="flag0" value="1" checked>
<label for="flag0">Flag 0 - [MQTT] Broadcast led params together (send dimmer and color when dimmer or color changes, topic name: YourDevName/led_basecolor_rgb/get, YourDevName/led_dimmer/get)</label>
</div>
*/
        hprintf128(request, "<div><input type=\"checkbox\" name=\"flag%i\" id=\"flag%i\" value=\"1\"%s>",
            i, i, (CFG_HasFlag(i) ?" checked":"")); //this is less that 128 char

        hprintf128(request,"<label for=\"flag%i\">Flag %i - ",i,i);
        poststr(request, flagName);
        poststr(request,"</label></div>");
	}
	poststr(request,"<input type=\"hidden\" id=\"setFlags\" name=\"setFlags\" value=\"1\">");
	poststr(request,"<input type=\"submit\" value=\"Submit\"></form>");

    poststr(request,"<form action=\"/cfg_generic\">\
            <label for=\"boot_ok_delay\">Uptime seconds required to mark boot as ok:</label><br>\
            <input type=\"text\" id=\"boot_ok_delay\" name=\"boot_ok_delay\" value=\"");
    hprintf128(request, "%i",CFG_GetBootOkSeconds());
    poststr(request,"\"><br>");
    poststr(request,"<input type=\"submit\" value=\"Save\"/></form>");

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}
int http_fn_cfg_startup(http_request_t *request) {
	int channelIndex;
	int newValue;
    int i;
	char tmpA[128];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Config startup");
	hprintf128(request,"<h5>Here you can set pin start values<h5>");
	hprintf128(request,"<h5>For relays, simply use 1 or 0</h5>");
	hprintf128(request,"<h5>For 'remember last power state', use -1 as a special value</h5>");
	hprintf128(request,"<h5>For dimmers, range is 0 to 100</h5>");
	hprintf128(request,"<h5>For custom values, you can set any number you want to</h5>");
	hprintf128(request,"<h5>Remember that you can also use short startup command to run commands like led_baseColor #FF0000 and led_enableAll 1 etc</h5>");
	hprintf128(request,"<h5><color=red>Remembering last state of LED driver is not yet fully supported, please wait!</color></h5>");

    if(	http_getArg(request->url,"idx",tmpA,sizeof(tmpA))) {
		channelIndex = atoi(tmpA);
		if(	http_getArg(request->url,"value",tmpA,sizeof(tmpA))) {
			newValue = atoi(tmpA);


			CFG_SetChannelStartupValue(channelIndex,newValue);
			// also save current value if marked as saved
			Channel_SaveInFlashIfNeeded(channelIndex);
			hprintf128(request,"<h5>Setting channel %i start value to %i<h5>",channelIndex,newValue);

			CFG_Save_IfThereArePendingChanges();
		}
    }

	for(i = 0; i < CHANNEL_MAX; i++) {
		if(CHANNEL_IsInUse(i)) {
			int startValue;

			startValue = CFG_GetChannelStartupValue(i);

			poststr(request,"<br><form action=\"/cfg_startup\">\
					<label for=\"boot_ok_delay\">New start value for channel ");
			hprintf128(request, "%i",i);
			poststr(request,":</label><br>\
					<input type=\"hidden\" id=\"idx\" name=\"idx\" value=\"");
			hprintf128(request, "%i",i);
			poststr(request,"\">");
			poststr(request,"<input type=\"number\" id=\"value\" name=\"value\" value=\"");
			hprintf128(request, "%i",startValue);
			poststr(request,"\"><br>");
			poststr(request,"<input type=\"submit\" value=\"Save\"/></form>");
		}
	}

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_cfg_dgr(http_request_t *request) {
	char tmpA[128];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Device groups");

	hprintf128(request,"<h5>Here you can configure Tasmota Device Groups<h5>");


    if(	http_getArg(request->url,"name",tmpA,sizeof(tmpA))) {
		int newSendFlags;
		int newRecvFlags;

		newSendFlags = 0;
		newRecvFlags = 0;

		if(http_getArgInteger(request->url,"s_pwr"))
			newSendFlags |= DGR_SHARE_POWER;
		if(http_getArgInteger(request->url,"r_pwr"))
			newRecvFlags |= DGR_SHARE_POWER;
		if(http_getArgInteger(request->url,"s_lbr"))
			newSendFlags |= DGR_SHARE_LIGHT_BRI;
		if(http_getArgInteger(request->url,"r_lbr"))
			newRecvFlags |= DGR_SHARE_LIGHT_BRI;
		if(http_getArgInteger(request->url,"s_lcl"))
			newSendFlags |= DGR_SHARE_LIGHT_COLOR;
		if(http_getArgInteger(request->url,"r_lcl"))
			newRecvFlags |= DGR_SHARE_LIGHT_COLOR;

		CFG_DeviceGroups_SetName(tmpA);
		CFG_DeviceGroups_SetSendFlags(newSendFlags);
		CFG_DeviceGroups_SetRecvFlags(newRecvFlags);

		if(tmpA[0] != 0) {
#ifndef OBK_DISABLE_ALL_DRIVERS
			DRV_StartDriver("DGR");
#endif
		}
		CFG_Save_IfThereArePendingChanges();
    }
	{
		int newSendFlags;
		int newRecvFlags;
		const char *groupName = CFG_DeviceGroups_GetName();


		newSendFlags = CFG_DeviceGroups_GetSendFlags();
		newRecvFlags = CFG_DeviceGroups_GetRecvFlags();

		poststr(request,"<form action=\"/cfg_dgr\"><label for=\"name\">Group name:</label><br><input type=\"text\" id=\"name\" name=\"name\" value=\"");
		poststr(request,groupName);
		poststr(request,"\"><br><table><tr><th>Name</th><th>Tasmota Code</th><th>Receive</th><th>Send</th></tr><tr><td>Power</td><td>1</td>");

		poststr(request,"     <td><input type=\"checkbox\" name=\"r_pwr\" value=\"1\"");
		if(newRecvFlags & DGR_SHARE_POWER)
			poststr(request," checked");
		poststr(request,"></td>  <td><input type=\"checkbox\" name=\"s_pwr\" value=\"1\"");
		if(newSendFlags & DGR_SHARE_POWER)
			poststr(request," checked");
		poststr(request,"></td> ");

		poststr(request,"  </tr>  <tr>    <td>Light Brightness</td>    <td>2</td>");

		poststr(request,"    <td><input type=\"checkbox\" name=\"r_lbr\" value=\"1\"");
		if(newRecvFlags & DGR_SHARE_LIGHT_BRI)
			poststr(request," checked");
		poststr(request,"></td>    <td><input type=\"checkbox\" name=\"s_lbr\" value=\"1\"");
		if(newSendFlags & DGR_SHARE_LIGHT_BRI)
			poststr(request," checked");
		poststr(request,"></td> ");

		poststr(request,"  </tr>  <tr>    <td>Light Color</td>    <td>16</td>");
		poststr(request,"     <td><input type=\"checkbox\" name=\"r_lcl\" value=\"1\"");
		if(newRecvFlags & DGR_SHARE_LIGHT_COLOR)
			poststr(request," checked");
		poststr(request,"></td> <td><input type=\"checkbox\" name=\"s_lcl\" value=\"1\"");
		if(newSendFlags & DGR_SHARE_LIGHT_COLOR)
			poststr(request," checked");
		poststr(request,"></td> ");

		poststr(request,"    </tr></table>  <input type=\"submit\" value=\"Submit\"></form>");
	}

    poststr(request,htmlFooterReturnToCfgLink);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

void XR809_RequestOTAHTTP(const char *s);

int http_fn_ota_exec(http_request_t *request) {
	char tmpA[128];
	//char tmpB[64];

    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "OTA request");
    if(http_getArg(request->url,"host",tmpA,sizeof(tmpA))) {
        hprintf128(request,"<h3>OTA requested for %s!</h3>",tmpA);
    	addLogAdv(LOG_INFO, LOG_FEATURE_HTTP,"http_fn_ota_exec: will try to do OTA for %s \r\n",tmpA);
#if WINDOWS

#elif PLATFORM_BL602

#elif PLATFORM_W800
    t_http_fwup(tmpA);
#elif PLATFORM_XR809
    XR809_RequestOTAHTTP(tmpA);
#else
    otarequest(tmpA);
#endif
    }
    poststr(request,htmlFooterReturnToMenu);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_ota(http_request_t *request) {
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "OTA system");
    poststr(request,"<p>Simple OTA system (you should rather use the OTA from App panel where you can drag and drop file easily without setting up server). Use RBL file for OTA. In the OTA below, you should paste link to RBL file (you need HTTP server).</p>");
    poststr(request,"<form action=\"/ota_exec\">\
            <label for=\"host\">URL for new bin file:</label><br>\
            <input type=\"text\" id=\"host\" name=\"host\" value=\"");
    poststr(request,"\"><br>\
            <input type=\"submit\" value=\"Submit\" onclick=\"return confirm('Are you sure?')\">\
        </form> ");
    poststr(request,htmlFooterReturnToMenu);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}

int http_fn_other(http_request_t *request) {
    http_setup(request, httpMimeTypeHTML);
    http_html_start(request, "Not found");
    poststr(request,"Not found.<br/>");
    poststr(request,htmlFooterReturnToMenu);
    http_html_end(request);
	poststr(request, NULL);
    return 0;
}
