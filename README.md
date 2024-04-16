# check-journal

**check-journal** checks journals whether new logs are available, then reports them.
It can filter logs with any of systemd unit, priority, syslog facility or regexp.

There are two mode: *Standard* mode or *Sensu plugin* mode.
It switches exclusive by whether `--check[=NUM]` option is passed or not.

* **Standard**: behaves as like grep(1). Logs will be printed to standard output.
* **Sensu plugin**: is almost same as Standard mode, except error reporting and exit status.

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
  -i --ignore-case
  -v --invert-match=PATTERN
     --check[=NUM]
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
