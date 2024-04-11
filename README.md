# check-journal

## Installation

```console
$ make
$ ./check-journal --state-file=out [--user]
```

## Synopsis

```
check-journal [options]

options:
  -f --state-file=FILE
     --user
  -u --unit=UNIT
  -p --priority=PRIORITY
     --facility=FACILITY
  -e --regexp=PATTERN
  -h --help
```

### Priorities

* **0**, **emerg**
* **1**, **alert**
* **2**, **crit**
* **3**, **err**
* **4**, **warning**
* **5**, **notice**
* **6**, **info**
* **7**, **debug**

### Facilities

* **0**, **kern**
* **1**, **user**
* **2**, **mail**
* **3**, **daemon**
* **4**, **auth**
* **5**, **syslog**
* **6**, **lpr**
* **7**, **news**
* **8**, **uucp**
* **9**, **cron**
* **10**, **authpriv**
* **11**, **ftp**
* **16**, **local0**
* **17**, **local1**
* **18**, **local2**
* **19**, **local3**
* **20**, **local4**
* **21**, **local5**
* **22**, **local6**
* **23**, **local7**
