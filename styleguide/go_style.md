# Go Style Guide

## **Gofmt**

Run [gofmt](https://golang.org/cmd/gofmt/) on your code to automatically fix the majority of mechanical style issues. Almost all Go code in the wild uses gofmt. The rest of this document addresses non-mechanical style points.

## **Comments**

Doc comments should be a complete sentence, beginning with the name of the thing being described and ending in a period:

```go
// Compile parses a regular expression and returns, if successful,
// a Regexp that can be used to match against text.
func Compile(str string) (*Regexp, error) {
```

## **Naming**

### **Packages**

When a package is imported, the package name becomes an accessor for the contents. It's helpful if everyone using the package can use the same name to refer to its contents, which implies that the package name should be good: short, concise, evocative. By convention, packages are given lower case, single-word names; there should be no need for underscores or mixedCaps.

Another convention is that the package name is the base name of its source directory; the package in src/encoding/base64 is imported as "encoding/base64" but has name base64, not encoding_base64 and not encodingBase64.

### **Getters**

It's neither idiomatic nor necessary to put Get into a getter's name. If you have a field called owner (lower case, unexported), the getter method should be called Owner (upper case, exported), not GetOwner. The use of upper-case names for export provides the hook to discriminate the field from the method. A setter function, if needed, will likely be called SetOwner.

## **References**

We based many of the design decisions from the [golang wiki](https://github.com/golang/go/wiki/CodeReviewComments)
