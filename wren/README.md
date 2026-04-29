# Start

Wren does not include a native IO module—this functionality is provided by a separate project, wren-cil.

As a result, the calculator cannot run as a standalone REPL. However, we can leverage Wren’s embeddability to integrate it with C and build a fully functional REPL that way.

```console
$ wren main.wren
```

# Refs

- [wren.io](https://wren.io/)
