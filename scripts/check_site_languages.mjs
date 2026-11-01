// Run with node scripts/check_site_languages.mjs (no external dependencies).
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
const source = readFileSync(new URL('../docs/assets/language.js', import.meta.url), 'utf8');
function run({lang='en', browser='en-US', saved=null, query='', hash='', blocked=false, prefix='/study/'}={}) {
  const storage = new Map(saved ? [['nuvus-lang', saved]] : []);
  const href = `https://example.test${prefix}${lang==='de'?'de/':''}index.html${query}${hash}`;
  const location = {href, search:query, hash, replace(url){this.redirect=url;}};
  const callbacks = {};
  const links = ['en','de'].map(language=>({getAttribute(){return language;}}));
  const document = {
    documentElement:{lang}, currentScript:{src:`https://example.test${prefix}assets/language.js`},
    querySelectorAll(){return links;}, addEventListener(name,fn){callbacks[name]=fn;}
  };
  vm.runInNewContext(source, {URL, location, document, navigator:{languages:[browser],language:browser},
    localStorage:{getItem(k){if(blocked)throw Error('blocked');return storage.get(k);},setItem(k,v){if(blocked)throw Error('blocked');storage.set(k,v);}},
    window:{addEventListener(name,fn){callbacks[name]=fn;}}
  });
  callbacks.DOMContentLoaded?.();
  return {location,storage,links,callbacks};
}
assert.equal(run({browser:'de-DE'}).location.redirect,'https://example.test/study/de/index.html');
assert.equal(run({browser:'de-AT',hash:'#claims',query:'?source=test'}).location.redirect,'https://example.test/study/de/index.html?source=test#claims');
assert.equal(run({lang:'de',browser:'fr-FR'}).location.redirect,'https://example.test/study/index.html');
assert.equal(run({lang:'de',browser:'de-CH'}).location.redirect,undefined);
assert.equal(run({browser:'de-DE',saved:'en'}).location.redirect,undefined);
assert.equal(run({browser:'en-US',saved:'de'}).location.redirect,'https://example.test/study/de/index.html');
const explicit=run({browser:'de-DE',saved:'de',query:'?lang=en',hash:'#observers'});
assert.equal(explicit.location.redirect,undefined);assert.equal(explicit.storage.get('nuvus-lang'),'en');
assert.equal(explicit.links[1].href,'https://example.test/study/de/index.html?lang=de#observers');
explicit.location.hash='#timing';explicit.callbacks.hashchange();
assert.equal(explicit.links[1].href,'https://example.test/study/de/index.html?lang=de#timing');
assert.equal(run({lang:'de',browser:'en',query:'?lang=de',blocked:true}).location.redirect,undefined);
assert.equal(run({browser:'de',blocked:true,prefix:'/'}).location.redirect,'https://example.test/de/index.html');
assert.equal(run({browser:'de-DE',saved:'invalid',query:'?lang=invalid'}).location.redirect,'https://example.test/study/de/index.html?lang=invalid');
console.log('PASS: browser defaults, regional locales, fallback, preference precedence, anchors, subpath hosting and blocked storage');
const german = readFileSync(new URL('../docs/de/index.html', import.meta.url), 'utf8');
assert.ok(german.includes('lang="de"'));
assert.ok(german.includes('data-lang-link="de" lang="de" hreflang="de" aria-current="page"'));
assert.ok(!german.includes('</code>und') && !german.includes('</code>oder'));
const catalogue = JSON.parse(readFileSync(new URL('./translations/de.json', import.meta.url), 'utf8'));
for (const [en, de] of Object.entries(catalogue)) {
  assert.deepEqual(en.match(/\d+(?:\.\d+)?/g), de.match(/\d+(?:\.\d+)?/g), `Numeric translation mismatch: ${en}`);
}
console.log('PASS: German page language, active switch, inline spacing and numeric parity');
