/*
 * $Id: interface.c,v 1.34 2003/07/09 16:14:39 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <ctype.h>

#include "libgift.h"

#include "network.h"

/* #define PRETTY_PRINT */
#include "interface.h"

/*****************************************************************************/

enum _token_type
{
	TOKEN_TEXT = 0,
	TOKEN_SPACE,                       /* ' ' */
	TOKEN_PAREN_OPEN,                  /* '(' */
	TOKEN_PAREN_CLOSE,                 /* ')' */
	TOKEN_BRACK_OPEN,                  /* '[' */
	TOKEN_BRACK_CLOSE,                 /* ']' */
	TOKEN_BRACE_OPEN,                  /* '{' */
	TOKEN_BRACE_CLOSE,                 /* '}' */
	TOKEN_SEMICOLON                    /* ';' */
};

struct _token
{
	char            *str;
	enum _token_type type;
};

/*****************************************************************************/

static void inode_free (InterfaceNode *inode)
{
	free (inode->key);
	free (inode->keydisp);
	free (inode->value);
	free (inode);
}

static int inode_valid (InterfaceNode *inode)
{
	char *key;

	if (!inode || !inode->key || !inode->keydisp)
		return FALSE;

	key = inode->keydisp;

	/* first character can not be a number */
	if (!isalpha (*key++))
		return FALSE;

	for (; *key; key++)
	{
		if (!isalnum (*key))
			return FALSE;
	}

	return TRUE;
}

static InterfaceNode *inode_new (char *key, char *value)
{
	InterfaceNode *inode;
	char *ptr;

	if (!key)
		return NULL;

	if (!(inode = MALLOC (sizeof (InterfaceNode))))
		return NULL;

	inode->key     = STRDUP (key);
	inode->keydisp = STRDUP (key);
	inode->value   = STRDUP (value);

	if ((ptr = strchr (inode->keydisp, '[')))
		*ptr = 0;

	if (!inode_valid (inode))
	{
		inode_free (inode);
		return NULL;
	}

	return inode;
}

/*****************************************************************************/

Interface *interface_new (char *command, char *value)
{
	Interface *p;

	if (!(p = malloc (sizeof (Interface))))
		return NULL;

	p->command = STRDUP (command);
	p->value   = STRDUP (value);
	p->tree    = NULL;

	return p;
}

static void free_key (TreeNode *node, void *udata, int depth)
{
	inode_free (node->data);
}

void interface_free (Interface *p)
{
	if (!p)
		return;

	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach)free_key, NULL);
	tree_destroy (&p->tree);

	free (p->command);
	free (p->value);
	free (p);
}

/*****************************************************************************/

static void set_data (char **data, char *value)
{
	free (*data);
	*data = STRDUP (value);
}

void interface_set_command (Interface *p, char *command)
{
	if (!p)
		return;

	set_data (&p->command, command);
}

void interface_set_value (Interface *p, char *value)
{
	if (!p)
		return;

	set_data (&p->value, value);
}

/*****************************************************************************/

static int keypathcmp (InterfaceNode *inode, char *key)
{
	return strcasecmp (inode->key, key);
}

static TreeNode *lookup (Interface *p, char *key)
{
	TreeNode *node = NULL;
	TreeNode *st;
	char     *keydup, *keydup0;
	char     *token;

	/* string_sep is going to modify key, so we have to make sure that this
	 * is safe */
	if (!p || !(keydup = STRDUP (key)))
		return NULL;

	/* string_sep will modify its argument, so we must track the beginning */
	keydup0 = keydup;

	while ((token = string_sep (&keydup, "/")))
	{
		st = (node ? node->child : NULL);

		node = tree_find (&p->tree, st, FALSE,
		                  (TreeNodeCompare)keypathcmp, token);

		if (!node)
			break;
	}

	free (keydup0);
	return node;
}

char *interface_get (Interface *p, char *key)
{
	TreeNode      *node;
	InterfaceNode *inode;

	if (!(node = lookup (p, key)) || !(inode = node->data))
		return NULL;

	/* we don't want to permit NULL her so that the caller can differentiate
	 * between empty value and non-existant key */
	return STRING_NOTNULL (inode->value);
}

/*****************************************************************************/

BOOL interface_put (Interface *p, char *key, char *value)
{
	TreeNode      *node;
	InterfaceNode *inode;
	char          *keypath;
	char          *keyname;

	if (!p || !(keypath = STRDUP (key)))
		return FALSE;

	if ((keyname = strrchr (keypath, '/')))
		*keyname++ = 0;
	else
	{
		keyname = keypath;
		keypath = NULL;
	}

	node = lookup (p, keypath);

	if ((inode = inode_new (keyname, value)))
		inode->node = tree_insert (&p->tree, node, NULL, inode);

	free (keypath ? keypath : keyname);

	return TRUE;
}

/*****************************************************************************/

static void foreach_wrapper (TreeNode *node, void **data, int depth)
{
	Interface       *p       = data[0];
	InterfaceForeach func    = data[1];
	void            *udata   = data[2];
	InterfaceNode   *inode   = node->data;

	(*func) (p, inode, udata);
}

static void interface_foreach_node (Interface *p, TreeNode *node,
                                    InterfaceForeach func, void *udata)
{
	void *data[] = { p, func, udata };

	if (!p || !func)
		return;

	/* refuse to process nodes that exist, but are without children */
	if (node && !node->child)
		return;

	tree_foreach (&p->tree, (node ? node->child : NULL), 0, FALSE,
	              (TreeForeach)foreach_wrapper, data);
}

void interface_foreach_ex (Interface *p, InterfaceNode *inode,
                           InterfaceForeach func, void *udata)
{
	if (!inode || !inode->node)
		return;

	interface_foreach_node (p, inode->node, func, udata);
}

void interface_foreach (Interface *p, char *key,
                        InterfaceForeach func, void *udata)
{
	TreeNode *node;

	node = lookup (p, key);

	if (key && !node)
		return;

	interface_foreach_node (p, node, func, udata);
}

/*****************************************************************************/

static struct _token *new_token (enum _token_type type, char *str)
{
	struct _token *token;

	if (!(token = malloc (sizeof (struct _token))))
		return NULL;

	token->type = type;
	token->str  = str;

	return token;
}

static void free_token (struct _token *token)
{
	free (token->str);
	free (token);
}

static enum _token_type is_special (char c, enum _token_type context)
{
	enum _token_type type = TOKEN_TEXT;

	if (isspace (c))
		c = ' ';

	switch (c)
	{
	 case '(': type = TOKEN_PAREN_OPEN;  break;
	 case ')': type = TOKEN_PAREN_CLOSE; break;
	 case '[': type = TOKEN_BRACK_OPEN;  break;
	 case ']': type = TOKEN_BRACK_CLOSE; break;
	 case '{': type = TOKEN_BRACE_OPEN;  break;
	 case '}': type = TOKEN_BRACE_CLOSE; break;
	 case ';': type = TOKEN_SEMICOLON;   break;
	 case ' ':
		if (context == TOKEN_TEXT)
			type = TOKEN_SPACE;
		break;
	}

	return type;
}

static char *escape (char *str)
{
	String *escaped;
	char   *ptr;

	if (!str || !(*str))
		return STRDUP (str);

	if (!(escaped = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	for (ptr = str; *ptr; ptr++)
	{
		switch (*ptr)
		{
		 case '(':
		 case ')':
		 case '[':
		 case ']':
		 case '{':
		 case '}':
		 case '\\':
		 case ';':
			string_appendf (escaped, "\\%c", *ptr);
			break;
		 default:
			string_appendc (escaped, *ptr);
			break;
		}
	}

	return string_free_keep (escaped);
}

static char *unescape (char *str)
{
	char *pwrite;
	char *pread;

	for (pread = pwrite = str; *pread; pread++, pwrite++)
	{
		if (*pread == '\\')
			pread++;

		/* weinholt: ha. */
		if (pwrite != pread)
			*pwrite = *pread;
	}

	*pwrite = '\0';

	return str;
}

static struct _token *get_token (char **packet, enum _token_type context)
{
	enum _token_type  type;
	char             *text;
	char             *ptr;
	char             *str;

	if (!(str = *packet) || !str[0])
		return NULL;

	/* shift past senseless whitespace */
	while ((is_special (*str, context)) == TOKEN_SPACE)
		str++;

	/* first things first, lets check if we're currently sitting on some kind
	 * of special character */
	if ((type = is_special (*str, context)) != TOKEN_TEXT)
	{
		(*packet) = str + 1;
		return new_token (type, STRDUP_N (str, 1));
	}

	/* gobble up the waiting text token */
	for (ptr = str; *ptr; ptr++)
	{
		/* skip over the next char */
		if (*ptr == '\\')
		{
			ptr++;
			continue;
		}

		if (is_special (*ptr, context) != TOKEN_TEXT)
			break;
	}

	*packet = ptr;

	if (!(text = STRDUP_N (str, (ptr - str))))
		text = STRDUP ("");

	string_trim (text);
	unescape (text);

	return new_token (TOKEN_TEXT, text);
}

/*****************************************************************************/

static int last_depth = 0;

static void indent (TreeNode *node, String *s, int depth)
{
#ifdef PRETTY_PRINT
#if 0
	/* not the command */
	if (!node || (node->prev || node->parent))
		string_append (s, "  ");
#endif

	while (depth-- > 0)
		string_append (s, "  ");
#endif /* PRETTY_PRINT */
}

static void show_depth (TreeNode *node, String *s,
                        int depth, int *depth_last)
{
	char after_block;
	int  depth_prev;
	int  depth_in;

	depth_prev = depth;
	depth_in   = depth;

#ifdef PRETTY_PRINT
	after_block = '\n';
#else /* !PRETTY_PRINT */
	after_block = ' ';
#endif /* PRETTY_PRINT */

	if (depth == *depth_last)
		indent (node, s, depth);
	else if (depth > *depth_last)
	{
		indent (node, s, *depth_last);

		while (depth-- > *depth_last)
		{
			string_appendf (s, "{%c", after_block);
			indent (node, s, depth_in++);
		}
	}
	else /* if (depth < *depth_last) */
	{
		indent (node, s, *depth_last - 1);

		while (depth++ < *depth_last)
		{
			string_appendf (s, "}%c", after_block);
			indent (node, s, depth_in--);
		}
	}

	*depth_last = depth_prev;
}

static void append_escaped (String *s, char *fmt, char *str)
{
	char *escaped;

	escaped = escape (str);
	string_appendf (s, fmt, escaped);
	free (escaped);
}

static void appendnode (String *s, char *key, char *value)
{
	char after_block;

	append_escaped (s, "%s", key);

#if 0
	if (inode->attr && inode->attr[0])
		append_escaped (s, "[%s]", inode->attr);
#endif

	if (value && *value)
		append_escaped (s, "(%s)", value);

#ifdef PRETTY_PRINT
	after_block = '\n';
#else
	after_block = ' ';
#endif /* PRETTY_PRINT */

	string_appendc (s, after_block);
}

static void build (TreeNode *node, String *s, int depth)
{
	InterfaceNode *inode;

	if (!(inode = node->data))
		return;

	show_depth (node, s, depth + 1, &last_depth);
	appendnode (s, inode->keydisp, inode->value);
}

String *interface_serialize (Interface *p)
{
	String *s;
	int     shift_back;

	if (!p || !(s = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	/* put the command information */
	appendnode (s, p->command, p->value);
	last_depth = 1;

	/* then put the children */
	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach) build, s);
	show_depth (NULL, s, 0, &last_depth);

	/* determine how much data can safely be rewound */
#ifdef PRETTY_PRINT
	shift_back = 3;
#else
	shift_back = 3; /* 1; */
#endif

	/* rewind the trailing space(s) for cleanliness */
	if (s->len >= shift_back)
		s->len -= shift_back;

	string_append (s, ";\n");

	return s;
}

/*****************************************************************************/

static TreeNode *flush (Tree **tree, TreeNode *node, TreeNode **r_last,
                        InterfaceNode *inode, InterfaceNode **r_inode)
{
	TreeNode *last = NULL;

	if (!inode)
		return NULL;

	if (!tree_find (tree, node, TRUE, NULL, inode))
	{
		last = tree_insert (tree, node, NULL, inode);
		inode->node = last;
	}

	if (r_inode)
		*r_inode = NULL;

	if (r_last && last)
		*r_last = last;

	return last;
}

static int parse (Interface *p, TreeNode *node, char **data)
{
	TreeNode         *last    = NULL;
	enum _token_type  context = TOKEN_TEXT;
	InterfaceNode    *inode   = NULL;
	struct _token    *token;

	while ((token = get_token (data, context)))
	{
		switch (token->type)
		{
		 case TOKEN_TEXT:
			{
				switch (context)
				{
				 case TOKEN_TEXT:
					{
						flush (&p->tree, node, &last, inode, &inode);

						if (p->command)
							inode = inode_new (token->str, NULL);
						else
							p->command = STRDUP (token->str);
					}
					break;
				 case TOKEN_PAREN_OPEN:
				 case TOKEN_BRACK_OPEN:
					{
						char **attr;

						if (!inode && !p->command)
						{
							free_token (token);
							return FALSE;
						}

						if (!inode)
							attr = &p->value;
						else
						{
							/*
							 * TOKEN_BRACK_OPEN is no longer valid, but we
							 * can't really remove all the extra cruft it
							 * introduces here quite yet.
							 */
							attr = (context == TOKEN_PAREN_OPEN) ?
								&inode->value : NULL;
						}

						if (attr)
							*attr = STRDUP (token->str);
					}
					break;
				 default:
					break;
				}
			}
			break;
		 case TOKEN_PAREN_OPEN:
		 case TOKEN_BRACK_OPEN:
			/* wth, the syntax has to look something like (foo(... for this
			 * to happen */
			if (context == TOKEN_PAREN_OPEN ||
				context == TOKEN_BRACK_OPEN)
			{
				free_token (token);
				return FALSE;
			}

			context = token->type;
			break;
		 case TOKEN_PAREN_CLOSE:
		 case TOKEN_BRACK_CLOSE:
			context = TOKEN_TEXT;
			break;
		 case TOKEN_BRACE_OPEN:
			{
				/* NOTE: we arent really flushing here because
				 * META { ... } (x) is technically valid syntax, but we need
				 * this entry in the tree for parenting */
				flush (&p->tree, node, &last, inode, NULL);
				if (!parse (p, last, data))
				{
					free_token (token);
					return FALSE;
				}
			}
			break;
		 case TOKEN_BRACE_CLOSE:
		 case TOKEN_SEMICOLON:
			{
				flush (&p->tree, node, &last, inode, &inode);
				free_token (token);

				/* make sure this semi-colon isn't lazily breaking the
				 * command */
				return (context == TOKEN_TEXT);
			}
			break;
		 default:
			break;
		}

		free_token (token);
	}

	if (inode)
		inode_free (inode);

	/* abnormal end-of-input, consider this a fatal error condition */
	return FALSE;
}

Interface *interface_unserialize (char *data, size_t len)
{
	Interface *p;
	char      *datadup, *datadup0;
	int        ret = FALSE;

	if (!(p = interface_new (NULL, NULL)))
		return NULL;

	/* TODO: this NEEDS to be fixed! */
	if ((datadup = STRDUP_N (data, len)))
	{
		datadup0 = datadup;
		ret = parse (p, NULL, &datadup);
		free (datadup0);
	}

	if (!ret)
	{
		interface_free (p);
		return NULL;
	}

	return p;
}

/*****************************************************************************/

int interface_send (Interface *p, TCPC *c)
{
	String *s;
	int     len;

	if (!c || !(s = interface_serialize (p)))
		return -1;

	len = tcp_write (c, s->str, s->len);
	string_free (s);

	return len;
}

/*****************************************************************************/

#if 0
static void foreachdump (TreeNode *node, void *data, int depth)
{
	InterfaceNode *inode = node->data;

	while (depth-- > 0)
		printf ("  ");

	printf ("%s = %s\n", inode->key, inode->value);
}

static void interface_dump (Interface *p)
{
	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach)foreachdump, NULL);
}
#endif

/*****************************************************************************/

#if 0
static Interface *build_packet ()
{
	Interface *p = NULL;

	if (!(p = interface_new ("search", stringf ("%d", 4))))
		return NULL;

	interface_put (p, "query", "foo (bar)");
	interface_put (p, "test", "123");
	interface_put (p, "test", "456");
	interface_put (p, "test", "789");
	interface_put (p, "meta", NULL);
	interface_put (p, "meta/bitrate", ">=192");
	interface_put (p, "meta/foo", "bar");
	interface_put (p, "source[0]", NULL);
	interface_put (p, "source[0]/user", "someguy");
	interface_put (p, "source[1]", NULL);
	interface_put (p, "source[1]/user", "someotherguy");
	interface_put (p, "source[1]/foo", NULL);
	interface_put (p, "source[1]/foo/bar", "baz");

	return p;
}

static void ffunc (Interface *p, InterfaceNode *node, void *udata)
{
	printf ("%s = %s\n", node->key, node->value);
}

static void show_source (Interface *p, InterfaceNode *node, void *udata)
{
	printf ("\t\t%i: %s = %s\n",
	        node->node->child ? TRUE : FALSE, node->key,
			STRING_NOTNULL(node->value));
}

static void show_sources (Interface *p, InterfaceNode *node, void *udata)
{
	if (strcasecmp (node->key, "SOURCE"))
		return;

	printf ("\t%s\n", node->key);
	interface_foreach_ex (p, node, (InterfaceForeach)show_source, NULL);
}

int main ()
{
	Interface *p;
	String    *s;

	p = build_packet ();

	printf ("FOREACH 1\n");
	interface_foreach (p, "Meta", (InterfaceForeach)ffunc, NULL);
	printf ("\n");

	s = interface_serialize (p);
	interface_free (p);

	printf ("PASS 1\n%s\n", s->str);

	p = interface_unserialize (s->str, s->len);
	string_free (s);
	s = interface_serialize (p);

	printf ("FOREACH 2\n");
	interface_foreach (p, NULL, (InterfaceForeach)show_sources, NULL);
	printf ("\n");

	interface_free (p);

	printf ("PASS 2\n%s\n", s->str);

	string_free (s);

	return 0;
}
#endif

#if 0
int main ()
{
	Interface *p;
	char      *s;
	String    *str;

	/*
	 * search query(foo\(bar\));            literal
	 * search query(foo\\\(bar\\\);         escaped for interface.c
	 * search query(foo\\\\\\(bar\\\\\\));  escaped for C
	 */
	s = "search query(foo\\\\\\(bar\\\\\\));";
	p = interface_unserialize (s, strlen (s));
	str = interface_serialize (p);
	printf ("%s\n", str->str);
	p = interface_unserialize (str->str, str->len);
	printf ("%p\n", p);

	return 0;
}
#endif
