// settings.js — Node-RED settings for the IoT stack.
// The ONLY non-default setting: expose Node's core 'crypto' and 'fs' modules to
// function nodes via functionGlobalContext, so the Lab 08 ECC decryption node can
// do global.get('crypto') / global.get('fs'). Function nodes cannot require() core
// modules directly, which is why this is needed (and why it is committed, to keep
// the stack config-as-code).
module.exports = {
    flowFile: 'flows.json',
    functionGlobalContext: {
        crypto: require('crypto'),
        fs: require('fs')
    }
};
