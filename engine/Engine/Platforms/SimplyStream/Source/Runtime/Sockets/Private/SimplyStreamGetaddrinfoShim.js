// SimplyStream wasm — pointer-ABI-safe getaddrinfo() shim (registered with emcc via --js-library by the SimplyStream
// toolchain, AFTER the stock library so this definition wins — same override mechanism as SimplyStreamSetjmpShim.js).
//
// WHY THIS EXISTS
// In MEMORY64 modes, pointer stores go through BigInt(...). Stock emscripten
// getaddrinfo (emsdk src/lib/libcore.js) stores the `canon` value into the ai_canonname POINTER field with
// makeSetValue(..., '*'). On the hostname-resolution path (and the no-node path) it passes canon = null; on the
// numeric-IP path it passes canon = node (a JS *string*). Under MEMORY64 makeSetValue('*') expands to
// `HEAPU64[...] = BigInt(canon)`, so:
//     BigInt(null)            -> TypeError: Cannot convert null to a BigInt
//     BigInt("34.173.62.173") -> SyntaxError: Cannot convert 34.173.62.173 to a BigInt
// The first fires the moment Lyra resolves a dedicated-server hostname on the Quick Play / MP join path, crashing
// the wasm (getaddrinfo is __proxy:'sync', so it aborts inside __emscripten_receive_on_main_thread_js and the
// runtime restarts and re-crashes on retry). On wasm32 the same store silently coerced to 0, which is why native
// builds and the wasm32 era never saw it — only a real MEMORY64 browser boot of the MP path exposes it.
//
// THE FIX
// ai_canonname is never populated with a real malloc'd char* by this implementation (all three call sites pass
// null or a bare string, not a heap buffer), so the POSIX-correct value here is NULL (0). We reproduce stock
// getaddrinfo verbatim and change exactly one line: the ai_canonname store now writes 0 instead of `canon`, which
// is valid for both MEMORY64 and native wasm32 because makeSetValue emits the active pointer representation. It
// loses no real behavior. If a future emscripten actually allocates a canonname buffer, revisit this override.
//
// Kept in lock-step with emsdk/upstream/emscripten/src/lib/libcore.js:getaddrinfo — the {{{ }}} macros are
// resolved against the *builder's* emsdk struct-info at link time, so this stays correct across emsdk bumps as
// long as the surrounding logic there is unchanged. If that function changes materially, re-sync this copy.
'use strict';

mergeInto(LibraryManager.library, {
  getaddrinfo__deps: ['$Sockets', '$DNS', '$inetPton4', '$inetNtop4', '$inetPton6', '$inetNtop6', '$writeSockaddr', 'malloc', 'htonl'],
  getaddrinfo__proxy: 'sync',
  getaddrinfo: (node, service, hint, out) => {
    // Note getaddrinfo currently only returns a single addrinfo with ai_next defaulting to NULL. When NULL
    // hints are specified or ai_family set to AF_UNSPEC or ai_socktype or ai_protocol set to 0 then we
    // really should provide a linked list of suitable addrinfo values.
    var addrs = [];
    var canon = null;
    var addr = 0;
    var port = 0;
    var flags = 0;
    var family = {{{ cDefs.AF_UNSPEC }}};
    var type = 0;
    var proto = 0;
    var ai, last;

    function allocaddrinfo(family, type, proto, canon, addr, port) {
      var sa, salen, ai;
      var errno;

      salen = family === {{{ cDefs.AF_INET6 }}} ?
        {{{ C_STRUCTS.sockaddr_in6.__size__ }}} :
        {{{ C_STRUCTS.sockaddr_in.__size__ }}};
      addr = family === {{{ cDefs.AF_INET6 }}} ?
        inetNtop6(addr) :
        inetNtop4(addr);
      sa = _malloc(salen);
      errno = writeSockaddr(sa, family, addr, port);
      assert(!errno);

      ai = _malloc({{{ C_STRUCTS.addrinfo.__size__ }}});
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_family, 'family', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_socktype, 'type', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_protocol, 'proto', 'i32') }}};
      // Store NULL (0), never the JS null/string `canon`; makeSetValue emits the active pointer representation.
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_canonname, '0', '*') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addr, 'sa', '*') }}};
      if (family === {{{ cDefs.AF_INET6 }}}) {
        {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addrlen, C_STRUCTS.sockaddr_in6.__size__, 'i32') }}};
      } else {
        {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addrlen, C_STRUCTS.sockaddr_in.__size__, 'i32') }}};
      }
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_next, '0', 'i32') }}};

      return ai;
    }

    if (hint) {
      flags = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_flags, 'i32') }}};
      family = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_family, 'i32') }}};
      type = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_socktype, 'i32') }}};
      proto = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_protocol, 'i32') }}};
    }
    if (type && !proto) {
      proto = type === {{{ cDefs.SOCK_DGRAM }}} ? {{{ cDefs.IPPROTO_UDP }}} : {{{ cDefs.IPPROTO_TCP }}};
    }
    if (!type && proto) {
      type = proto === {{{ cDefs.IPPROTO_UDP }}} ? {{{ cDefs.SOCK_DGRAM }}} : {{{ cDefs.SOCK_STREAM }}};
    }

    // If type or proto are set to zero in hints we should really be returning multiple addrinfo values, but for
    // now default to a TCP STREAM socket so we can at least return a sensible addrinfo given NULL hints.
    if (proto === 0) {
      proto = {{{ cDefs.IPPROTO_TCP }}};
    }
    if (type === 0) {
      type = {{{ cDefs.SOCK_STREAM }}};
    }

    if (!node && !service) {
      return {{{ cDefs.EAI_NONAME }}};
    }
    if (flags & ~({{{ cDefs.AI_PASSIVE }}}|{{{ cDefs.AI_CANONNAME }}}|{{{ cDefs.AI_NUMERICHOST }}}|
        {{{ cDefs.AI_NUMERICSERV }}}|{{{ cDefs.AI_V4MAPPED }}}|{{{ cDefs.AI_ALL }}}|{{{ cDefs.AI_ADDRCONFIG }}})) {
      return {{{ cDefs.EAI_BADFLAGS }}};
    }
    if (hint !== 0 && ({{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_flags, 'i32') }}} & {{{ cDefs.AI_CANONNAME }}}) && !node) {
      return {{{ cDefs.EAI_BADFLAGS }}};
    }
    if (flags & {{{ cDefs.AI_ADDRCONFIG }}}) {
      // TODO
      return {{{ cDefs.EAI_NONAME }}};
    }
    if (type !== 0 && type !== {{{ cDefs.SOCK_STREAM }}} && type !== {{{ cDefs.SOCK_DGRAM }}}) {
      return {{{ cDefs.EAI_SOCKTYPE }}};
    }
    if (family !== {{{ cDefs.AF_UNSPEC }}} && family !== {{{ cDefs.AF_INET }}} && family !== {{{ cDefs.AF_INET6 }}}) {
      return {{{ cDefs.EAI_FAMILY }}};
    }

    if (service) {
      service = UTF8ToString(service);
      port = parseInt(service, 10);

      if (isNaN(port)) {
        if (flags & {{{ cDefs.AI_NUMERICSERV }}}) {
          return {{{ cDefs.EAI_NONAME }}};
        }
        // TODO support resolving well-known service names from:
        // http://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt
        return {{{ cDefs.EAI_SERVICE }}};
      }
    }

    if (!node) {
      if (family === {{{ cDefs.AF_UNSPEC }}}) {
        family = {{{ cDefs.AF_INET }}};
      }
      if ((flags & {{{ cDefs.AI_PASSIVE }}}) === 0) {
        if (family === {{{ cDefs.AF_INET }}}) {
          addr = _htonl({{{ cDefs.INADDR_LOOPBACK }}});
        } else {
          addr = [0, 0, 0, _htonl(1)];
        }
      }
      ai = allocaddrinfo(family, type, proto, null, addr, port);
      {{{ makeSetValue('out', '0', 'ai', '*') }}};
      return 0;
    }

    //
    // try as a numeric address
    //
    node = UTF8ToString(node);
    addr = inetPton4(node);
    if (addr !== null) {
      // incoming node is a valid ipv4 address
      if (family === {{{ cDefs.AF_UNSPEC }}} || family === {{{ cDefs.AF_INET }}}) {
        family = {{{ cDefs.AF_INET }}};
      }
      else if (family === {{{ cDefs.AF_INET6 }}} && (flags & {{{ cDefs.AI_V4MAPPED }}})) {
        addr = [0, 0, _htonl(0xffff), addr];
        family = {{{ cDefs.AF_INET6 }}};
      } else {
        return {{{ cDefs.EAI_NONAME }}};
      }
    } else {
      addr = inetPton6(node);
      if (addr !== null) {
        // incoming node is a valid ipv6 address
        if (family === {{{ cDefs.AF_UNSPEC }}} || family === {{{ cDefs.AF_INET6 }}}) {
          family = {{{ cDefs.AF_INET6 }}};
        } else {
          return {{{ cDefs.EAI_NONAME }}};
        }
      }
    }
    if (addr != null) {
      ai = allocaddrinfo(family, type, proto, node, addr, port);
      {{{ makeSetValue('out', '0', 'ai', '*') }}};
      return 0;
    }
    if (flags & {{{ cDefs.AI_NUMERICHOST }}}) {
      return {{{ cDefs.EAI_NONAME }}};
    }

    //
    // try as a hostname
    //
    // resolve the hostname to a temporary fake address
    node = DNS.lookup_name(node);
    addr = inetPton4(node);
    if (family === {{{ cDefs.AF_UNSPEC }}}) {
      family = {{{ cDefs.AF_INET }}};
    } else if (family === {{{ cDefs.AF_INET6 }}}) {
      addr = [0, 0, _htonl(0xffff), addr];
    }
    ai = allocaddrinfo(family, type, proto, null, addr, port);
    {{{ makeSetValue('out', '0', 'ai', '*') }}};
    return 0;
  },
});
