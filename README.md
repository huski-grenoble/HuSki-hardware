# HuSki Hardware

Présentation du projet : https://air.imag.fr/index.php/PROJET-1FO5_1819_SkiLocator

Partie android : https://github.com/huski-grenoble/HuSki

Code réalisé par FOMBARON Quentin (HuCard) et FERREIRA Joffrey (HuConnect) au cour du PFE de Polytech Grenoble spécialité Informatique (ex RICM)

## HuConnect 

Code pour une carte Heltech Wifi 32 LoRa développé sur arduino ide contenant les fonctionnalités suivantes :
* Connexion bluetooth avec l'application
* Communication LoRa
* Gestion des cartes appareillés (ajout, suppression, modifier la fréquence d'émission des HuCard entre 2 modes)
* Calcul et envoie du RSSI paquet reçu

Pour le niveau de batterie il est codé en brute car nous ne pouvions pas réaliser le diviseur de tension pour la HuConnect

## HuCard - Quentin

Code pour une carte Heltec Wifi 32 LoRa avec un module GPS, un diviseur de tension, une batterie et un régulateur de tension développé sur Arduino IDE proposant les fonctionnalitées suivantes : 
* Géolocalisation GPS (altitude, longitude, latitude)
* Calcul et envoie du niveau de batterie sur 5 niveaux
* Possède plusieurs mode de fonctionnement (activation, désactivation et envoie toutes les 2 minutes ou 2 secondes)

![HuCard_Ext](https://i.imgur.com/o610FY3.jpg)
> Boitier HuCard designé et imprimé en 3D par mes soins, avec une entrée micro USB pour la recharge et un QRCode pour l'identification sur l'applicaiton.

![HuCard_Int](https://i.imgur.com/dfIyMy4.jpg)
> Intérieur du boitier HuCard avec la carte HelTech et son écran de debug, elle-même sur une carte de prototypage composée du diviseur et régulateur de tension. La batterie située juste au dessus du module GPS. Les antennes (LoRa et GPS) sont sur le capot avec l'intérupteur général.
