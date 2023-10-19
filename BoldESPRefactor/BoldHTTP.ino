#include "BoldCommon.h"

//werkt niet meer volgens deze manier!!
int authenticeer(UnlockData *theLock)
{
  Serial.println("NO auth build in");
  return -1;
  if (WiFi.status() == WL_CONNECTED)
  {

    HTTPClient http;
    String url = apiUrl + "v1/authentications/" + myLock.authToken;
    http.begin(url);
    http.addHeader("X-Auth-Token", myLock.authToken);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "gzip");
    http.addHeader("User-Agent", "okhttp/3.14.9");

    DEBUG_PRINT("Authenticeer");

    int httpResponseCode = http.PUT("{\"appDescription\":\"Bold Smart Lock - Android application\",\"appId\":\"sesam.technology.bold\",\"appName\":\"Bold Smart Lock\",\"appVersion\":\"2.2.0a (453)\",\"clientDescription\":\"<REPLACE_ME>\",\"clientId\":<REPLACE_ME>,\"clientLocale\":\"en_US\",\"clientType\":\"Android\",\"clientVersion\":\"29\",\"language\":\"en\",\"notificationToken\":\"<REPLACE_ME>\"}");

  
    if (httpResponseCode > 0)
    {

      String response = http.getString();
      // char outputData[38];

      int res = getInterestingData(response, "token", theLock->authToken);

      DEBUG_PRINT(httpResponseCode);
      DEBUG_PRINT(response);
      DEBUG_PRINT("Outputdata: ");
      DEBUG_PRINT(theLock->authToken);
      http.end();

      return res;
    }
    else
    {

      Serial.print("Error on sending PUT Request: ");
      Serial.println(httpResponseCode);
      return 0;
    }

    http.end();
  }
  else
  {
    Serial.println("Error in WiFi connection");
    return 0;
  }
}

int getInterestingData(String response, String wanted, char *outputBuffer)
{
  deserializeJson(doc, response); 
  
  JsonObject object = doc.as<JsonObject>();

  String cmdPayload = object[wanted];
  if (cmdPayload.length() > 1)
  {
    DEBUG_PRINT("response was a succes for string: " + wanted);
    DEBUG_PRINT(cmdPayload.length());
    memcpy(outputBuffer, cmdPayload.c_str(), cmdPayload.length());
    DEBUG_PRINT(cmdPayload);
    return cmdPayload.length();
  }
  else
  {
    Serial.println("requested data not found");
    return 0;
  }
}

//mogelijk werkt het ook niet meer?
int makeRequest(String apiUrl, int sort, String interestingData[], int lengthInterestingData, String httpData, char *outputData[])
{
  if (WiFi.status() == WL_CONNECTED)
  {

    HTTPClient http;

    http.begin(apiUrl);
    http.addHeader("Authorization", String("Bearer ") + String(myLock.authToken));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "gzip");
    http.addHeader("User-Agent", "okhttp/3.14.9");
    int httpResponseCode = 0;

    switch (sort)
    {
      case 1:
        httpResponseCode = http.GET();
        break;
      case 2:
        httpResponseCode = http.POST(httpData);
        break;
      case 3:
        httpResponseCode = http.PUT(httpData);
        break;
    }

    if (httpResponseCode > 0)
    {

      String response = http.getString();
      response.remove(0, 1); // verwijder [ en ]
      response.remove(-1, 1);
      int result = 0;
      for (int i = 0; i < lengthInterestingData; i++)
      {
        result = getInterestingData(response, interestingData[i], outputData[i]);
        if (result == 0)
        { // when getting an error.
          break;
        }
      }
      return result;
    }
    else
    {

      Serial.print("Error on sending Request: ");
      Serial.println(httpResponseCode);
      Serial.println(apiUrl);
      http.end();

      return -1;
    }

    http.end();
  }
  else
  {
    Serial.println("Error in WiFi connection");
    return -1;
  }
  return -1;
}


//eigenlijk zijn deze twee commando's het handigste om ff zelf te bouwen en na te bootsen
int getSesamHandshake(UnlockData *theLock)
{

  // https://api.sesamtechnology.com/v1/controller/v0/handshakes?deviceId=<REPLACE_ME>

  String url = apiUrl + "v1/controller/v0/handshakes?deviceId=" + deviceID;

  String wantedData[] = {"handshakeKey", "payload"};
  char *outputData[] = {theLock->handShakeKey, theLock->handShakePayload};
  DEBUG_PRINT("GetSesamHandShake");
  return makeRequest(url, 1, wantedData, 2, "", outputData);
}


//eigenlijk zijn deze twee commando's het handigste om ff zelf te bouwen en na te bootsen.  je hebt de handshake nodig en de command payload

int getSesamCommand(UnlockData *theLock)
{
  String url = apiUrl + "v1/controller/v0/commands/activate-device?deviceId=" + deviceID;
  String wantedData[] = {"payload"};

  char *outputData[] = {theLock->commandPayload};
  DEBUG_PRINT("GetSesamCommand");
  return makeRequest(url, 1, wantedData, 1, "", outputData);
}
