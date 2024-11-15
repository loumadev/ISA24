Napište program imapcl, který umožní čtení elektronické pošty pomocí  protokolu IMAP4rev1 (RFC 3501). Program po spuštění stáhne zprávy uložené na serveru a uloží je do zadaného adresáře (každou zprávu zvlášť). Na standardní výstup vypíše počet stažených zpráv. Pomocí dodatečných parametrů je možné funkcionalitu měnit.

Při vytváření programu je povoleno použít hlavičkové soubory pro práci se sokety a další obvyklé funkce používané v síťovém prostředí (jako je `netinet/*`, `sys\*`, `arpa\*` apod.), knihovnu pro práci s vlákny (`pthread`), signály, časem, stejně jako standardní knihovnu jazyka C (varianty ISO/ANSI i POSIX), C++ a STL. Pro práci se SSL/TLS je povolena knihovna openssl. Jiné knihovny nejsou povoleny.

**Spuštění aplikace**

Použití:
```sh
imapcl server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir
```

Pořadí parametrů je libovolné. Popis parametrů:


- Povinně je uveden název serveru (IP adresa, nebo doménové jméno) požadovaného zdroje.
- Volitelný parametr `-p` port_ specifikuje číslo portu na serveru. Zvolte vhodnou výchozí hodnotu v závislosti na specifikaci parametru `-T` a číslech portů registrovaných organizací IANA.
- Parametr `-T` zapíná šifrování (imaps), pokud není parametr uveden,  použije se nešifrovaná varianta protokolu.
- Volitelný parametr `-c` soubor s certifikáty, který se použije pro ověření platnosti certifikátu SSL/TLS předloženého serverem.
- Volitelný parametr `-C` určuje adresář, ve kterém se mají vyhledávat certifikáty, které se použijí pro ověření platnosti certifikátu SSL/TLS předloženého serverem. Výchozí hodnota je `/etc/ssl/certs`.
- Při použití parametru parametru `-n` se bude pracovat (číst) pouze s novými zprávami.
- Při použití parametru `-h` se budou stahovat pouze hlavičky zpráv.
- Povinný parametr `-a auth_file` odkazuje na soubor s autentizaci (příkaz `LOGIN`), obsah konfiguračního souboru `auth_file` je zobrazený níže.
- Parametr `-b` specifikuje název schránky, se kterou se bude na serveru pracovat. Výchozí hodnota je `INBOX`.
- Povinný parametr `-o out_dir` specifikuje výstupní adresář, do kterého má program stažené zprávy uložit.

**Soubor s autentizačními údaji**

Konfigurační soubor s autentizačními údaji bude obsahovat uživatelské jméno a heslo v jednoduchém formátu:

```
username = jmeno
password = heslo
```

Uvažujte unixový textový soubor ukončený znakem nového řádku.

**Výstup aplikace**

Po spuštění aplikace vypište pouze informaci o počtu stažených zpráv, text sdělení nesmí být delší než 1 řádek. Text sdělení vhodně upravte v závislosti na použití parametrů `-n` a `-h`.

Jednotlivé zprávy budou ukládány ve formátu Internet Message Format (RFC 5322) do vhodně pojmenovaných souborů v adresáři specifikovaným parametrem `-o`, každá zpráva v samostatném souboru. Příklad obsahu souboru je:

```
Date: Wed, 14 Sep 2016 03:54:39 -0700
From: Sender <sender@example.com>
To: receiver@example.com
Subject: Message
Message-ID: <20160914035439.03264562@mininet-vm>

Toto je tělo e-mailu. Před tělem e-mailu jsou hlavičky a prázdný řádek.
```

**Příklad spuštění programu**


```sh
$ imapcl eva.fit.vutbr.cz -o maildir -a cred
Staženo 15 zpráv ze schránky INBOX.
```

```sh
$ imapcl 10.10.10.1 -p 9993 -T -n -b Important -o maildir -a cred
Staženy 2 nové zprávy ze schránky Important.
```

```sh
$ imapcl eva.fit.vutbr.cz -o maildir -a /dev/null
Není možné ověřit identitu serveru eva.fit.vutbr.cz.
```

**Referenční prostředí pro překlad**

Referenční překlad projektů proběhne na stroji merlin.fit.vutbr.cz.

**Doporučená literatura**

- [Registr portů transportních protokolů](http://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml)
- [RFC3501: INTERNET MESSAGE ACCESS PROTOCOL - VERSION 4rev1](https://tools.ietf.org/html/rfc3501)
- [RFC5322: Internet Message Format](https://tools.ietf.org/html/rfc5322)
- [Základy použití knihovny OpenSSL](http://www.ibm.com/developerworks/library/l-openssl/)

**Možná rozšíření projektu**

Implementaci rozšíření je možné kompenzovat drobné nedostatky v implementaci základní varianty. Mezi navrhovaná rozšíření patří podpora STARTTLS a interaktivní režim programu (viz níže).

V interaktivním režimu se program připojí ke schránce a bude běžet až do ukončení příkazem QUIT. Uživatel bude mít možnost stahovat zprávy příkazem DOWNLOAD(NEW|ALL) \[MAILDIR\], číst nové zprávy příkazem READNEW \[MAILDIR\]. Při přijetí zprávy na serveru uživatele vhodně informujte na standardním výstupu. Podle uvážení implementujte další příkazy. Interaktivní režím se aktivuje parametrem `-i`.

Příklad interaktivního režimu (řádky vypisované na standardní výstup programu jsou uvozeny znakem \*):

```sh
$ imapcl eva.fit.vutbr.cz  -a auth -o maildir -i

* Na serveru je 10 zpráv ve schránce INBOX, 3 nové.
* Na serveru je 5 zpráv ve schránce Important, 2 nové.
DOWNLOADNEW
* Staženy 3 nové zprávy ze schránky INBOX.
DOWNLOADNEW Important
* Staženy 2 nové zprávy ze schránky Important.
DOWNLOADALL
* Staženo 10 zpráv ze schránky INBOX.
DOWNLOADALL Important
* Staženo 5 zpráv ze schránky Important.
READNEW
* Zprávy ve schránce INBOX označeny jako přečtené.
DOWNLOADNEW
* Ve schránce INBOX není žádná nová zpráva.
* Přijata 1 nová zpráva ve schránce INBOX.
* Přijaty 2 nové zprávy ve schránce INBOX.
DOWNLOADNEW
* Staženy 2 nové zprávy ze schránky INBOX.
```