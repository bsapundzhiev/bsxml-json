# Fairly simple and portable XML DOM and JSON implementation in C

You can do both parsing and creating DOM and JSON type objects.

## Build

**NOTE:** XML Parser is wrapper around expat library.  
`USE_EXPAT=1` in Makefile comment it out for none(only json).

Build as static library with 'gnu make':
```bash
$ sudo apt-get install libexpat1-dev
$ make lib
```

## License

See COPYING file for copying permission.