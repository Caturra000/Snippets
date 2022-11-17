// 来自其它object，因此这个符号需要暴露出来，才能被当前object用到
extern void mustBeKnownButNotAnAPI();

void api() {
    mustBeKnownButNotAnAPI();
}
