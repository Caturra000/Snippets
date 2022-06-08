extern void mustBeKnownButNotAnAPI();

void __attribute__((visibility("default"))) api() {
    mustBeKnownButNotAnAPI();
}
