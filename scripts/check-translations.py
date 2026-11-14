#!/usr/bin/env python3
"""Validate full menu coverage and format placeholders for all offered locales."""
from pathlib import Path
import json,re,string,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
locales=['en-US','it','fr','de','es','pt','ko']
def flatten(value,prefix=''):
 result={}
 for key,item in value.items():
  if isinstance(item,dict):result.update(flatten(item,prefix+key+'/'))
  else:result[prefix+key]=item
 return result
base=flatten(json.loads((root/'resources/i18n/en-US/strings.json').read_text()))
extra=json.loads((root/'resources/i18n/en-US/extra.json').read_text())
def fields(text):return [name for _,name,_,_ in string.Formatter().parse(text) if name is not None]
for locale in locales:
 data=flatten(json.loads((root/f'resources/i18n/{locale}/strings.json').read_text()))
 assert data.keys()==base.keys(), (locale,base.keys()-data.keys(),data.keys()-base.keys())
 for key,value in data.items():
  assert isinstance(value,str) and value,(locale,key)
  assert fields(value)==fields(base[key]),(locale,key,fields(value),fields(base[key]))
 added=json.loads((root/f'resources/i18n/{locale}/extra.json').read_text())
 assert added.keys()==extra.keys(),locale
 assert all(isinstance(v,str) and v for v in added.values()),locale
for file in (root/'resources/xml').rglob('*.xml'):
 ET.fromstring(file.read_text().replace('brls:', 'brls_'))
 for key in re.findall(r'@i18n/([^"\s]+)',file.read_text()):assert key in base,(file,key)
print(f'Translations verified: {len(locales)} languages, {len(base)} menu keys + {len(extra)} additional messages each')
