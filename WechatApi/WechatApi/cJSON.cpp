/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* cJSON */
/* JSON parser in C. */
#include "pch.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"


static const wchar_t *ep;

const wchar_t *cJSON_GetErrorPtr(void) {return ep;}

static int cJSON_strcasecmp(const wchar_t *s1,const wchar_t *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; towlower(*s1) == towlower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return towlower(*(const wchar_t *)s1) - towlower(*(const wchar_t *)s2);
}

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static wchar_t* cJSON_strdup(const wchar_t* str)
{
      size_t len;
      wchar_t* copy;

      len = sizeof(wchar_t) * (wcslen(str) + 1);
      if (!(copy = (wchar_t*)cJSON_malloc(len))) return 0;
      memcpy(copy,str,len);
      return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks) { /* Reset hooks */
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }

	cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
	cJSON_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
	cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
	if (node) memset(node,0,sizeof(cJSON));
	return node;
}

/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *c)
{
	cJSON *next;
	while (c)
	{
		next=c->next;
		if (!(c->type&cJSON_IsReference) && c->child) cJSON_Delete(c->child);
		if (!(c->type&cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
		if (c->string) cJSON_free(c->string);
		cJSON_free(c);
		c=next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const wchar_t *parse_number(cJSON *item,const wchar_t *num)
{
	double n=0,sign=1,scale=0;int subscale=0,signsubscale=1;

	if (*num==L'-') sign=-1,num++;	/* Has sign? */
	if (*num==L'0') num++;			/* is zero */
	if (*num>=L'1' && *num<=L'9')	do	n=(n*10.0)+(*num++ -L'0');	while (*num>=L'0' && *num<=L'9');	/* Number? */
	if (*num==L'.' && num[1]>=L'0' && num[1]<=L'9') {num++;		do	n=(n*10.0)+(*num++ -L'0'),scale--; while (*num>=L'0' && *num<=L'9');}	/* Fractional part? */
	if (*num==L'e' || *num==L'E')		/* Exponent? */
	{	num++;if (*num==L'+') num++;	else if (*num==L'-') signsubscale=-1,num++;		/* With sign? */
		while (*num>=L'0' && *num<=L'9') subscale=(subscale*10)+(*num++ - L'0');	/* Number? */
	}

	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */
	
	item->valuedouble=n;
	item->valueint=(int)n;
	item->type=cJSON_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static wchar_t *print_number(cJSON *item)
{
	wchar_t *str;
	double d=item->valuedouble;
	if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN)
	{
		str=(wchar_t*)cJSON_malloc(21*sizeof(wchar_t));	/* 2^64+1 can be represented in 21 chars. */
		if (str) swprintf(str,L"%d",item->valueint);
	}
	else
	{
		str=(wchar_t*)cJSON_malloc(64*sizeof(wchar_t));	/* This is a nice tradeoff. */
		if (str)
		{
			if (fabs(floor(d)-d)<=DBL_EPSILON && fabs(d)<1.0e60)swprintf(str,L"%.0f",d);
			else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)			swprintf(str,L"%e",d);
			else												swprintf(str,L"%f",d);
		}
	}
	return str;
}

static unsigned parse_hex4(const wchar_t *str)
{
	unsigned h=0;
	if (*str>=L'0' && *str<=L'9') h+=(*str)-L'0'; else if (*str>=L'A' && *str<=L'F') h+=10+(*str)-L'A'; else if (*str>=L'a' && *str<=L'f') h+=10+(*str)-L'a'; else return 0;
	h=h<<4;str++;
	if (*str>=L'0' && *str<=L'9') h+=(*str)-L'0'; else if (*str>=L'A' && *str<=L'F') h+=10+(*str)-L'A'; else if (*str>=L'a' && *str<=L'f') h+=10+(*str)-L'a'; else return 0;
	h=h<<4;str++;
	if (*str>=L'0' && *str<=L'9') h+=(*str)-L'0'; else if (*str>=L'A' && *str<=L'F') h+=10+(*str)-L'A'; else if (*str>=L'a' && *str<=L'f') h+=10+(*str)-L'a'; else return 0;
	h=h<<4;str++;
	if (*str>=L'0' && *str<=L'9') h+=(*str)-L'0'; else if (*str>=L'A' && *str<=L'F') h+=10+(*str)-L'A'; else if (*str>=L'a' && *str<=L'f') h+=10+(*str)-L'a'; else return 0;
	return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
//static const unsigned wchar_t firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const wchar_t firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const wchar_t *parse_string(cJSON *item,const wchar_t *str)
{
	const wchar_t *ptr=str+1;wchar_t *ptr2;wchar_t *out;int len=0;unsigned uc,uc2;
	if (*str!=L'\"') {ep=str;return 0;}	/* not a string! */
	
	while (*ptr!=L'\"' && *ptr && ++len) if (*ptr++ == L'\\') ptr++;	/* Skip escaped quotes. */
	
	out=(wchar_t*)cJSON_malloc((len+1)*sizeof(wchar_t));	/* This is how long we need for the string, roughly. */
	if (!out) return 0;
	
	ptr=str+1;ptr2=out;
	while (*ptr!=L'\"' && *ptr)
	{
		if (*ptr!=L'\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case L'b': *ptr2++=L'\b';	break;
				case L'f': *ptr2++=L'\f';	break;
				case L'n': *ptr2++=L'\n';	break;
				case L'r': *ptr2++=L'\r';	break;
				case L't': *ptr2++=L'\t';	break;
				case L'u':	 /* transcode utf16 to utf8. */
					uc=parse_hex4(ptr+1);ptr+=4;	/* get the unicode char. */

					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)	break;	/* check for invalid.	*/

					if (uc>=0xD800 && uc<=0xDBFF)	/* UTF16 surrogate pairs.	*/
					{
						if (ptr[1]!=L'\\' || ptr[2]!=L'u')	break;	/* missing second-half of surrogate.	*/
						uc2=parse_hex4(ptr+3);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;	/* invalid second-half of surrogate.	*/
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}

					len=4;if (uc<0x80) len=1;else if (uc<0x800) len=2;else if (uc<0x10000) len=3; ptr2+=len;
					
					switch (len) {
						case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr==L'\"') ptr++;
	item->valuestring=out;
	item->type=cJSON_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static wchar_t *print_string_ptr(const wchar_t *str)
{
	const wchar_t *ptr;wchar_t *ptr2,*out;int len=0;wchar_t token;
	
	if (!str) return cJSON_strdup(L"");
	ptr=str;while ((token=*ptr) && ++len) {if (wcschr(L"\"\\\b\f\n\r\t",token)) len++; else if (token<32) len+=5;ptr++;}
	
	out=(wchar_t*)cJSON_malloc(sizeof(wchar_t)*(len+3));
	if (!out) return 0;

	ptr2=out;ptr=str;
	*ptr2++=L'\"';
	while (*ptr)
	{
		if ((wchar_t)*ptr>31 && *ptr!=L'\"' && *ptr!=L'\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++=L'\\';
			switch (token=*ptr++)
			{
				case L'\\':	*ptr2++=L'\\';	break;
				case L'\"':	*ptr2++=L'\"';	break;
				case L'\b':	*ptr2++=L'b';	break;
				case L'\f':	*ptr2++=L'f';	break;
				case L'\n':	*ptr2++=L'n';	break;
				case L'\r':	*ptr2++=L'r';	break;
				case L'\t':	*ptr2++=L't';	break;
				default: swprintf(ptr2,L"u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++=L'\"';*ptr2++=0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static wchar_t *print_string(cJSON *item)	{return print_string_ptr(item->valuestring);}

/* Predeclare these prototypes. */
static const wchar_t *parse_value(cJSON *item,const wchar_t *value);
static wchar_t *print_value(cJSON *item,int depth,int fmt);
static const wchar_t *parse_array(cJSON *item,const wchar_t *value);
static wchar_t *print_array(cJSON *item,int depth,int fmt);
static const wchar_t *parse_object(cJSON *item,const wchar_t *value);
static wchar_t *print_object(cJSON *item,int depth,int fmt);

/* Utility to jump whitespace and cr/lf */
static const wchar_t *skip(const wchar_t *in) {while (in && *in && *in<=L' ') in++; return in;}

/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const wchar_t *value,const wchar_t **return_parse_end,int require_null_terminated)
{
	const wchar_t *end=0;
	cJSON *c=cJSON_New_Item();
	ep=0;
	if (!c) return 0;       /* memory fail */

	end=parse_value(c,skip(value));
	if (!end)	{cJSON_Delete(c);return 0;}	/* parse failure. ep is set. */

	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {end=skip(end);if (*end) {cJSON_Delete(c);ep=end;return 0;}}
	if (return_parse_end) *return_parse_end=end;
	return c;
}
/* Default options for cJSON_Parse */
cJSON *cJSON_Parse(const wchar_t *value) {return cJSON_ParseWithOpts(value,0,0);}

/* Render a cJSON item/entity/structure to text. */
wchar_t *cJSON_Print(cJSON *item)				{return print_value(item,0,1);}
wchar_t *cJSON_PrintUnformatted(cJSON *item)	{return print_value(item,0,0);}

/* Parser core - when encountering text, process appropriately. */
static const wchar_t *parse_value(cJSON *item,const wchar_t *value)
{
	if (!value)						return 0;	/* Fail on null. */
	if (!wcsncmp(value,L"null",4))	{ item->type=cJSON_NULL;  return value+4; }
	if (!wcsncmp(value,L"false",5))	{ item->type=cJSON_False; return value+5; }
	if (!wcsncmp(value,L"true",4))	{ item->type=cJSON_True; item->valueint=1;	return value+4; }
	if (*value==L'\"')				{ return parse_string(item,value); }
	if (*value==L'-' || (*value>=L'0' && *value<=L'9'))	{ return parse_number(item,value); }
	if (*value==L'[')				{ return parse_array(item,value); }
	if (*value==L'{')				{ return parse_object(item,value); }

	ep=value;return 0;	/* failure. */
}

/* Render a value to text. */
static wchar_t *print_value(cJSON *item,int depth,int fmt)
{
	wchar_t *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
		case cJSON_NULL:	out=cJSON_strdup(L"null");	break;
		case cJSON_False:	out=cJSON_strdup(L"false");break;
		case cJSON_True:	out=cJSON_strdup(L"true"); break;
		case cJSON_Number:	out=print_number(item);break;
		case cJSON_String:	out=print_string(item);break;
		case cJSON_Array:	out=print_array(item,depth,fmt);break;
		case cJSON_Object:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

/* Build an array from input text. */
static const wchar_t *parse_array(cJSON *item,const wchar_t *value)
{
	cJSON *child;
	if (*value!=L'[')	{ep=value;return 0;}	/* not an array! */

	item->type=cJSON_Array;
	value=skip(value+1);
	if (*value==L']') return value+1;	/* empty array. */

	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;		 /* memory fail */
	value=skip(parse_value(child,skip(value)));	/* skip any spacing, get the value. */
	if (!value) return 0;

	while (*value==L',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item())) return 0; 	/* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	/* memory fail */
	}

	if (*value==L']') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an array to text */
static wchar_t *print_array(cJSON *item,int depth,int fmt)
{
	wchar_t **entries;
	wchar_t *out=0,*ptr,*ret;int len=5;
	cJSON *child=item->child;
	int numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) numentries++,child=child->next;
	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out=(wchar_t*)cJSON_malloc(3*sizeof(wchar_t));
		if (out) wcscpy(out,L"[]");
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries=(wchar_t**)cJSON_malloc(numentries*sizeof(wchar_t*));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(wchar_t*));
	/* Retrieve all the results: */
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=wcslen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}
	
	/* If we didn't fail, try to malloc the output string */
	if (!fail) out=(wchar_t*)cJSON_malloc(len*sizeof(wchar_t));
	/* If that fails, we fail. */
	if (!out) fail=1;

	/* Handle failure. */
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) cJSON_free(entries[i]);
		cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output array. */
	*out=L'[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		wcscpy(ptr,entries[i]);ptr+=wcslen(entries[i]);
		if (i!=numentries-1) {*ptr++=L',';if(fmt)*ptr++=L' ';*ptr=0;}
		cJSON_free(entries[i]);
	}
	cJSON_free(entries);
	*ptr++=L']';*ptr++=0;
	return out;	
}

/* Build an object from the text. */
static const wchar_t *parse_object(cJSON *item,const wchar_t *value)
{
	cJSON *child;
	if (*value!=L'{')	{ep=value;return 0;}	/* not an object! */
	
	item->type=cJSON_Object;
	value=skip(value+1);
	if (*value==L'}') return value+1;	/* empty array. */
	
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=L':') {ep=value;return 0;}	/* fail! */
	value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
	if (!value) return 0;
	
	while (*value==L',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item()))	return 0; /* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=L':') {ep=value;return 0;}	/* fail! */
		value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
		if (!value) return 0;
	}
	
	if (*value==L'}') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an object to text. */
static wchar_t *print_object(cJSON *item,int depth,int fmt)
{
	wchar_t **entries=0,**names=0;
	wchar_t *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	cJSON *child=item->child;
	int numentries=0,fail=0;
	/* Count the number of entries. */
	while (child) numentries++,child=child->next;
	/* Explicitly handle empty object case */
	if (!numentries)
	{
		out=(wchar_t*)cJSON_malloc((fmt?depth+4:3)*sizeof(wchar_t));
		if (!out)	return 0;
		ptr=out;*ptr++=L'{';
		if (fmt) {*ptr++=L'\n';for (i=0;i<depth-1;i++) *ptr++=L'\t';}
		*ptr++=L'}';*ptr++=0;
		return out;
	}
	/* Allocate space for the names and the objects */
	entries=(wchar_t**)cJSON_malloc(numentries*sizeof(wchar_t*));
	if (!entries) return 0;
	names=(wchar_t**)cJSON_malloc(numentries*sizeof(wchar_t*));
	if (!names) {cJSON_free(entries);return 0;}
	memset(entries,0,sizeof(wchar_t*)*numentries);
	memset(names,0,sizeof(wchar_t*)*numentries);

	/* Collect all the results into our arrays: */
	child=item->child;depth++;if (fmt) len+=depth;
	while (child)
	{
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=wcslen(ret)+wcslen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
	}
	
	/* Try to allocate the output string */
	if (!fail) out=(wchar_t*)cJSON_malloc(len*sizeof(wchar_t));
	if (!out) fail=1;

	/* Handle failure */
	if (fail)
	{
		for (i=0;i<numentries;i++) {if (names[i]) cJSON_free(names[i]);if (entries[i]) cJSON_free(entries[i]);}
		cJSON_free(names);cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output: */
	*out=L'{';ptr=out+1;if (fmt)*ptr++=L'\n';*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) for (j=0;j<depth;j++) *ptr++=L'\t';
		wcscpy(ptr,names[i]);ptr+=wcslen(names[i]);
		*ptr++=L':';if (fmt) *ptr++=L'\t';
		wcscpy(ptr,entries[i]);ptr+=wcslen(entries[i]);
		if (i!=numentries-1) *ptr++=',';
		if (fmt) *ptr++=L'\n';*ptr=0;
		cJSON_free(names[i]);cJSON_free(entries[i]);
	}
	
	cJSON_free(names);cJSON_free(entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++=L'\t';
	*ptr++=L'}';*ptr++=0;
	return out;	
}

/* Get Array size/item / object item. */
int    cJSON_GetArraySize(cJSON *array)							{cJSON *c=array->child;int i=0;while(c)i++,c=c->next;return i;}
cJSON *cJSON_GetArrayItem(cJSON *array,int item)				{cJSON *c=array->child;  while (c && item>0) item--,c=c->next; return c;}
cJSON *cJSON_GetObjectItem(cJSON *object,const wchar_t *string)	{cJSON *c=object->child; while (c && cJSON_strcasecmp(c->string,string)) c=c->next; return c;}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev,cJSON *item) {prev->next=item;item->prev=prev;}
/* Utility for handling references. */
static cJSON *create_reference(cJSON *item) {cJSON *ref=cJSON_New_Item();if (!ref) return 0;memcpy(ref,item,sizeof(cJSON));ref->string=0;ref->type|=cJSON_IsReference;ref->next=ref->prev=0;return ref;}

/* Add item to array/object. */
void   cJSON_AddItemToArray(cJSON *array, cJSON *item)						{cJSON *c=array->child;if (!item) return; if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}
void   cJSON_AddItemToObject(cJSON *object,const wchar_t *string,cJSON *item)	{if (!item) return; if (item->string) cJSON_free(item->string);item->string=cJSON_strdup(string);cJSON_AddItemToArray(object,item);}
void	cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)						{cJSON_AddItemToArray(array,create_reference(item));}
void	cJSON_AddItemReferenceToObject(cJSON *object,const wchar_t *string,cJSON *item)	{cJSON_AddItemToObject(object,string,create_reference(item));}

cJSON *cJSON_DetachItemFromArray(cJSON *array,int which)			{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   cJSON_DeleteItemFromArray(cJSON *array,int which)			{cJSON_Delete(cJSON_DetachItemFromArray(array,which));}
cJSON *cJSON_DetachItemFromObject(cJSON *object,const wchar_t *string) {int i=0;cJSON *c=object->child;while (c && cJSON_strcasecmp(c->string,string)) i++,c=c->next;if (c) return cJSON_DetachItemFromArray(object,i);return 0;}
void   cJSON_DeleteItemFromObject(cJSON *object,const wchar_t *string) {cJSON_Delete(cJSON_DetachItemFromObject(object,string));}

/* Replace array/object items with new ones. */
void   cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
	if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;cJSON_Delete(c);}
void   cJSON_ReplaceItemInObject(cJSON *object,const wchar_t *string,cJSON *newitem){int i=0;cJSON *c=object->child;while(c && cJSON_strcasecmp(c->string,string))i++,c=c->next;if(c){newitem->string=cJSON_strdup(string);cJSON_ReplaceItemInArray(object,i,newitem);}}

/* Create basic types: */
cJSON *cJSON_CreateNull(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_NULL;return item;}
cJSON *cJSON_CreateTrue(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_True;return item;}
cJSON *cJSON_CreateFalse(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_False;return item;}
cJSON *cJSON_CreateBool(int b)					{cJSON *item=cJSON_New_Item();if(item)item->type=b?cJSON_True:cJSON_False;return item;}
cJSON *cJSON_CreateNumber(double num)			{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_Number;item->valuedouble=num;item->valueint=(int)num;}return item;}
cJSON *cJSON_CreateString(const wchar_t *string){cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_String;item->valuestring=cJSON_strdup(string);}return item;}
cJSON *cJSON_CreateArray(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Array;return item;}
cJSON *cJSON_CreateObject(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Object;return item;}

/* Create Arrays: */
cJSON *cJSON_CreateIntArray(const int *numbers,int count)		{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateFloatArray(const float *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateDoubleArray(const double *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateStringArray(const wchar_t **strings,int count){int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}

/* Duplication */
cJSON *cJSON_Duplicate(cJSON *item,int recurse)
{
	cJSON *newitem,*cptr,*nptr=0,*newchild;
	/* Bail on bad ptr */
	if (!item) return 0;
	/* Create new item */
	newitem=cJSON_New_Item();
	if (!newitem) return 0;
	/* Copy over all vars */
	newitem->type=item->type&(~cJSON_IsReference),newitem->valueint=item->valueint,newitem->valuedouble=item->valuedouble;
	if (item->valuestring)	{newitem->valuestring=cJSON_strdup(item->valuestring);	if (!newitem->valuestring)	{cJSON_Delete(newitem);return 0;}}
	if (item->string)		{newitem->string=cJSON_strdup(item->string);			if (!newitem->string)		{cJSON_Delete(newitem);return 0;}}
	/* If non-recursive, then we're done! */
	if (!recurse) return newitem;
	/* Walk the ->next chain for the child. */
	cptr=item->child;
	while (cptr)
	{
		newchild=cJSON_Duplicate(cptr,1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {cJSON_Delete(newitem);return 0;}
		if (nptr)	{nptr->next=newchild,newchild->prev=nptr;nptr=newchild;}	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{newitem->child=newchild;nptr=newchild;}					/* Set newitem->child and move to it */
		cptr=cptr->next;
	}
	return newitem;
}

void cJSON_Minify(wchar_t *json)
{
	wchar_t *into=json;
	while (*json)
	{
		if (*json==L' ') json++;
		else if (*json==L'\t') json++;	// Whitespace characters.
		else if (*json==L'\r') json++;
		else if (*json==L'\n') json++;
		else if (*json==L'/' && json[1]==L'/')  while (*json && *json!=L'\n') json++;	// double-slash comments, to end of line.
		else if (*json==L'/' && json[1]==L'*') {while (*json && !(*json==L'*' && json[1]==L'/')) json++;json+=2;}	// multiline comments.
		else if (*json==L'\"'){*into++=*json++;while (*json && *json!=L'\"'){if (*json==L'\\') *into++=*json++;*into++=*json++;}*into++=*json++;} // string literals, which are \" sensitive.
		else *into++=*json++;			// All other characters.
	}
	*into=0;	// and null-terminate.
}