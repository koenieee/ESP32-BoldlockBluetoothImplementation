import requests
import base64
import json
import urllib.parse


#Letop, deze file werkt niet meer. Wat dit ding doet is inloggen op de webserver van bold, maar dat kan tegenwoordig ook via: 
#https://auth.boldsmartlock.com/ 
#Letop, open de developer console bij het versturen bij de laatste stap. Daarmee krijg je je benodigde gegevens (refresh token en auth token) en kun je praten met de api van bold!

#Curl command:
def pretty_print_POST(req):
    """
    At this point it is completely built and ready
    to be fired; it is "prepared".

    However pay attention at the formatting used in 
    this function because it is programmed to be pretty 
    printed and may differ from the actual request.
    """
    print('{}\n{}\r\n{}\r\n\r\n{}'.format(
        '-----------START-----------',
        req.method + ' ' + req.url,
        '\r\n'.join('{}: {}'.format(k, v) for k, v in req.headers.items()),
        req.body,
    ))


url = 'https://api.boldsmartlock.com/v1/'
phonenumber = ""
password = ""
emailCode = ""
encodedPass = password #urllib.parse.quote(password) #base64.b64encode(str.encode(password))
encodedPhone = phonenumber #urllib.parse.quote(phonenumber) #base64.b64encode(str.encode(phonenumber))

print("encoded pass: " + str(encodedPass))
print("encoded phone: " + str(encodedPhone))




dataStap1 = '{"phone":"'+phonenumber+'","language":"en","purpose":"EmailValidationWithCode"}'
headers = {'Content-Type': 'application/json','User-Agent': 'okhttp/4.9.2'}


stap1Req = requests.post(url+"validations", data=dataStap1, headers=headers)
print("Stap1")
print(stap1Req.text)


dataID = json.loads(stap1Req.text)["id"]

emailCode = input("Enter BOLD validation code: ")

dataStap2 = '{"code":"'+emailCode+'"}' 


stap2Req = requests.post(url+"validations/"+dataID, data=dataStap2, headers=headers)

print("Stap2")
print(stap2Req.text)



dataID = json.loads(stap2Req.text)["id"]



dataStap3 = {'grant_type':'password','client_id':'BoldApp','client_secret':'<REPLACE_ME>','username':encodedPhone,'password': encodedPass,'mfa_token':dataID}
headers = {'Content-Type': 'application/x-www-form-urlencoded','User-Agent': 'okhttp/4.9.2'}


stap3Req = requests.post("https://api.boldsmartlock.com/v2/oauth/token", data=dataStap3, headers=headers)
print("Stap3")
print(stap3Req.text)

finalID = json.loads(stap3Req.text)["access_token"]

print("Final ID: " + finalID);

input("Press Enter to continue...")


exit()
refresh_token = json.loads(stap3Req.text)["refresh_token"]

dataStap3 = {'grant_type':'refresh_token','client_id':'BoldApp','client_secret':'<REPLACE_ME>','refresh_token':refresh_token}
headers = {'Content-Type': 'application/x-www-form-urlencoded','User-Agent': 'okhttp/4.9.2'}

#optioneel
req = requests.Request('POST',"https://api.boldsmartlock.com/v2/oauth/token", data=dataStap3, headers=headers)
prepared = req.prepare()

stap3Req = requests.post("https://api.boldsmartlock.com/v2/oauth/token", data=dataStap3, headers=headers)
print("Stap3")
print(stap3Req.text)

