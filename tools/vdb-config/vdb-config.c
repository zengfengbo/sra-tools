/*==============================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include "vdb-config.vers.h"
#include "configure.h"

#include <kapp/main.h>

#include <vdb/vdb-priv.h> /* VDBManagerListExternalSchemaModules */
#include <vdb/manager.h> /* VDBManager */

#include <kfg/kfg-priv.h> /* KConfig */
#include <kfg/ngc.h> /* KNgcObjMakeFromFile */
#include <kfg/properties.h> /* KConfig_Get_Default_User_Path */
#include <kfg/repository.h> /* KConfigImportNgc */

#include <vfs/manager.h> /* VFSManagerMake */
#include <vfs/path-priv.h> /* KPathGetCWD */

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/impl.h> /* KDirectoryGetSysDir */
#include <kfs/kfs-priv.h> /* KSysDirOSPath */

#include <klib/log.h> /* LOGERR */
#include <klib/misc.h> /* is_iser_an_admin */
#include <klib/namelist.h>
#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/rc.h> /* RC */
#include <klib/text.h> /* strcase_cmp */

#include <sysalloc.h> /* redefine malloc etc calls */
#include <os-native.h> /* SHLX */

#include <assert.h>
#include <errno.h>
#include <stdio.h> /* scanf */
#include <stdlib.h> /* getenv */
#include <string.h> /* memset */

#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt, rc, \
        "$(name): $(msg)", "name=%s,msg=%s", name, msg)))
#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

#define ALIAS_ALL    "a"
#define OPTION_ALL   "all"
static const char* USAGE_ALL[] = { "print all information [default]", NULL };

#define ALIAS_CFG    "i"
#define OPTION_CFG   "interactive"
static const char* USAGE_CFG[] = {
    "create/update configuration",
    NULL };

#define ALIAS_CFM    NULL
#define OPTION_CFM   "interactive-mode"
static const char* USAGE_CFM[] = {
    "interactive mode: 'textual' or 'graphical' (default)",
    NULL };

#define ALIAS_DIR    "d"
#define OPTION_DIR   "load-path"
static const char* USAGE_DIR[] = { "print load path", NULL };

#define ALIAS_ENV    "e"
#define OPTION_ENV   "env"
static const char* USAGE_ENV[] = { "print shell variables", NULL };

#define ALIAS_FIL    "f"
#define OPTION_FIL   "files"
static const char* USAGE_FIL[] = { "print loaded files", NULL };

#define ALIAS_FIX    NULL
#define OPTION_FIX   "restore-defaults"
static const char* USAGE_FIX[] =
{ "create default or update existing user configuration", NULL };

#define ALIAS_IMP    NULL
#define OPTION_IMP   "import"
static const char* USAGE_IMP[] = { "import ngc file", NULL };

#define ALIAS_MOD    "m"
#define OPTION_MOD   "modules"
static const char* USAGE_MOD[] = { "print external modules", NULL };

#define ALIAS_OUT    "o"
#define OPTION_OUT   "output"
static const char* USAGE_OUT[] = { "output type: one of (x n), "
    "where 'x' is xml (default), 'n' is native", NULL };

#define ALIAS_PCF    "p"
#define OPTION_PCF   "cfg"
static const char* USAGE_PCF[] = { "print current configuration", NULL };

#define ALIAS_ROOT   NULL
#define OPTION_ROOT  "root"
static const char* USAGE_ROOT[] =
    { "enforce configuration update while being run by superuser", NULL };

#define ALIAS_SET    "s"
#define OPTION_SET   "set"
static const char* USAGE_SET[] = { "set configuration node value", NULL };

OptDef Options[] =
{                                         /* needs_value, required */
      { OPTION_ALL, ALIAS_ALL, NULL, USAGE_ALL, 1, false, false }
    , { OPTION_CFG, ALIAS_CFG, NULL, USAGE_CFG, 1, false, false }
    , { OPTION_CFM, ALIAS_CFM, NULL, USAGE_CFM, 1, true , false }
    , { OPTION_DIR, ALIAS_DIR, NULL, USAGE_DIR, 1, false, false }
    , { OPTION_ENV, ALIAS_ENV, NULL, USAGE_ENV, 1, false, false }
    , { OPTION_FIL, ALIAS_FIL, NULL, USAGE_FIL, 1, false, false }
    , { OPTION_FIX, ALIAS_FIX, NULL, USAGE_FIX, 1, false, false }
    , { OPTION_IMP, ALIAS_IMP, NULL, USAGE_IMP, 1, true , false }
    , { OPTION_MOD, ALIAS_MOD, NULL, USAGE_MOD, 1, false, false }
    , { OPTION_OUT, ALIAS_OUT, NULL, USAGE_OUT, 1, true , false }
    , { OPTION_PCF, ALIAS_PCF, NULL, USAGE_PCF, 1, false, false }
    , { OPTION_SET, ALIAS_SET, NULL, USAGE_SET, 1, true , false }
    , { OPTION_ROOT,ALIAS_ROOT,NULL, USAGE_ROOT,1, false, false }
};

rc_t CC UsageSummary (const char * progname) {
    return KOutMsg (
        "Usage:\n"
        "  %s [options] [<query> ...]\n"
        "\n"
        "Summary:\n"
        "  Manage VDB configuration\n"
        "\n", progname);
}

rc_t CC Usage(const Args* args) { 
    rc_t rc = 0;

    const char* progname = UsageDefaultName;
    const char* fullpath = UsageDefaultName;

    if (args == NULL) {
        rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    }
    else {
        rc = ArgsProgram(args, &fullpath, &progname);
    }

    UsageSummary(progname);

    KOutMsg ("\nOptions:\n");

    HelpOptionLine (ALIAS_ALL, OPTION_ALL, NULL, USAGE_ALL);
    HelpOptionLine (ALIAS_PCF, OPTION_PCF, NULL, USAGE_PCF);
    HelpOptionLine (ALIAS_FIL, OPTION_FIL, NULL, USAGE_FIL);
    HelpOptionLine (ALIAS_ENV, OPTION_ENV, NULL, USAGE_ENV);
    HelpOptionLine (ALIAS_MOD, OPTION_MOD, NULL, USAGE_MOD);
    KOutMsg ("\n");
    HelpOptionLine (ALIAS_SET, OPTION_SET, "name=value", USAGE_SET);
    KOutMsg ("\n");
    HelpOptionLine (ALIAS_CFG, OPTION_CFG, NULL, USAGE_CFG);
    HelpOptionLine (ALIAS_CFM, OPTION_CFM, "mode", USAGE_CFM);
    HelpOptionLine (ALIAS_FIX, OPTION_FIX, NULL, USAGE_FIX);
    KOutMsg ("\n");
    HelpOptionLine (ALIAS_IMP, OPTION_IMP, "ngc-file", USAGE_IMP);
    KOutMsg ("\n");
    HelpOptionLine (ALIAS_OUT, OPTION_OUT, "x | n", USAGE_OUT);
    KOutMsg ("\n");
    HelpOptionLine (ALIAS_ROOT,OPTION_ROOT,NULL, USAGE_ROOT);

    KOutMsg ("\n");

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

const char UsageDefaultName[] = "vdb-config";

ver_t CC KAppVersion(void) { return VDB_CONFIG_VERS; }

static void Indent(bool xml, int n) {
    if (!xml)
    {   return; }
    while (n--)
    {   OUTMSG(("  ")); }
}

static rc_t KConfigNodeReadData(const KConfigNode* self,
    char* buf, size_t blen, size_t* num_read)
{
    rc_t rc = 0;
    size_t remaining = 0;
    assert(buf && blen && num_read);
    rc = KConfigNodeRead(self, 0, buf, blen, num_read, &remaining);
    assert(remaining == 0); /* TODO overflow check */
    assert(*num_read <= blen);
    return rc;
}

static
rc_t _printNodeData(const char *name, const char *data, size_t dlen)
{
    const char ticket[] = "download-ticket";
    size_t l = sizeof ticket - 1;
    if (string_cmp(name, string_measure(name, NULL),
        ticket, l, (uint32_t)l) == 0)
    {
        const char *ellipsis = "";
        const char replace[] =
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
        if (dlen > 70) {
            dlen = 70;
            ellipsis = "...";
        }
        return OUTMSG(("%.*s%s", (uint32_t)dlen, replace, ellipsis));
    }
    else {
        return OUTMSG(("%.*s", dlen, data));
    }
}

#define VDB_CONGIG_OUTMSG(args) do { if (xml) { OUTMSG(args); } } while (false)
static rc_t KConfigNodePrintChildNames(bool xml, const KConfigNode* self,
    const char* name, int indent, const char* aFullpath)
{
    rc_t rc = 0;
    uint32_t count = 0;
    int i = 0;
    char buffer[512] = "";
    size_t num_read = 0;
    bool hasChildren = false;
    bool hasData = false;
    const KConfigNode* node = NULL;
    KNamelist* names = NULL;
    assert(self && name);

    if (rc == 0)
    {   rc = KConfigNodeOpenNodeRead(self, &node, "%s", name);  }
    if (rc == 0) {
        rc = KConfigNodeReadData(node, buffer, sizeof buffer, &num_read);
        hasData = num_read > 0;
        if (hasData) {
 /* VDB_CONGIG_OUTMSG(("\n%s = \"%.*s\"\n\n", aFullpath, num_read, buffer)); */
        }
    }
    if (rc == 0)
    {   rc = KConfigNodeListChild(node, &names); }
    if (rc == 0) {
        rc = KNamelistCount(names, &count);
        hasChildren = count;
    }

    Indent(xml, indent);
    VDB_CONGIG_OUTMSG(("<%s", name));
    if (!hasChildren && !hasData)
    {   VDB_CONGIG_OUTMSG(("/>\n")); }
    else
    {   VDB_CONGIG_OUTMSG((">")); }
    if (hasData) {
        if (xml) {
            _printNodeData(name, buffer, num_read);
        }
        else {
            OUTMSG(("%s = \"", aFullpath));
            _printNodeData(name, buffer, num_read);
            OUTMSG(("\"\n"));
        }
    }
    if (hasChildren)
    {   VDB_CONGIG_OUTMSG(("\n"));}

    if (hasChildren) {
        for (i = 0; i < (int)count && rc == 0; ++i) {
            char* fullpath = NULL;
            const char* name = NULL;
            rc = KNamelistGet(names, i, &name);
            if (rc == 0) {
                size_t bsize = strlen(aFullpath) + 1 + strlen(name) + 1;
                fullpath = malloc(bsize + 1);
                if (fullpath == NULL) {
                    rc = RC
                        (rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                }
                else {
                    string_printf(fullpath, bsize, NULL,
                        "%s/%s", aFullpath, name);
                }
            }
            if (rc == 0) {
                rc = KConfigNodePrintChildNames
                    (xml, node, name, indent + 1, fullpath);
            }
            free(fullpath);
        }
    }

    if (hasChildren)
    {   Indent(xml, indent); }
    if (hasChildren || hasData)
    {   VDB_CONGIG_OUTMSG(("</%s>\n",name)); }

    RELEASE(KNamelist, names);
    RELEASE(KConfigNode, node);
    return rc;
}

typedef struct Params {
    Args* args;
    uint32_t argsParamIdx;
    uint32_t argsParamCnt;

    bool xml;

    const char* setValue;

    const char* ngc;

    bool modeSetNode;
    bool modeConfigure;
    EConfigMode configureMode;
    bool modeCreate;
    bool modeShowCfg;
    bool modeShowEnv;
    bool modeShowFiles;
    bool modeShowLoadPath;
    bool modeShowModules;
    bool modeRoot;

    bool showMultiple;
} Params;
static rc_t ParamsConstruct(int argc, char* argv[], Params* prm) {
    rc_t rc = 0;
    Args* args = NULL;
    int count = 0;
    assert(argc && argv && prm);
    memset(prm, 0, sizeof *prm);
    args = prm->args;
    do {
        uint32_t pcount = 0;
        rc = ArgsMakeAndHandle(&args, argc, argv, 1,
            Options, sizeof Options / sizeof (OptDef));
        if (rc) {
            LOGERR(klogErr, rc, "While calling ArgsMakeAndHandle");
            break;
        }
        prm->args = args;
        rc = ArgsParamCount(args, &prm->argsParamCnt);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get query parameter[s]");
            break;
        }
        if (prm->argsParamCnt > 0) {
            prm->modeShowCfg = true;
            ++count;
        }

    {   // OPTION_OUT
        prm->xml = true;
        rc = ArgsOptionCount(args, OPTION_OUT, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_OUT "' argument");
            break;
        }
        if (pcount) {
            const char* dummy = NULL;
            rc = ArgsOptionValue(args, OPTION_OUT, 0, &dummy);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_OUT "' argument");
                break;
            }
            if (!strcmp(dummy, "n")) {
                prm->xml = false;
            }
            else if (strcmp(dummy, "x")) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                LOGERR(klogErr, rc, "Bad " OPTION_OUT " value");
                break;
            }
        }
    }
    {   // OPTION_ENV
        rc = ArgsOptionCount(args, OPTION_ENV, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_ENV "' argument");
            break;
        }
        if (pcount) {
            prm->modeShowEnv = true;
            ++count;
        }
    }
    {   // OPTION_FIL
        rc = ArgsOptionCount(args, OPTION_FIL, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_FIL "' argument");
            break;
        }
        if (pcount) {
            prm->modeShowFiles = true;
            ++count;
        }
    }
    {   // OPTION_IMP
        rc = ArgsOptionCount(args, OPTION_IMP, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_IMP "' argument");
            break;
        }
        if (pcount) {
            rc = ArgsOptionValue(args, OPTION_IMP, 0, &prm->ngc);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_IMP "' argument");
                break;
            }
            else {
                 prm->modeShowCfg = false;
            }
        }
    }
    {   // OPTION_MOD
        rc = ArgsOptionCount(args, OPTION_MOD, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_MOD "' argument");
            break;
        }
        if (pcount) {
            prm->modeShowModules = true;
            ++count;
        }
    }
#if 0
    {   // OPTION_NEW
        rc = ArgsOptionCount(args, OPTION_NEW, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_NEW "' argument");
            break;
        }
        if (pcount) {
            prm->modeCreate = true;
            ++count;
        }
    }
#endif
    {   // OPTION_PCF
        rc = ArgsOptionCount(args, OPTION_PCF, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_PCF "' argument");
            break;
        }
        if (pcount) {
            if (!prm->modeShowCfg) {
                prm->modeShowCfg = true;
                ++count;
            }
        }
    }
    {   // OPTION_DIR
        rc = ArgsOptionCount(args, OPTION_DIR, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_DIR "' argument");
            break;
        }
        if (pcount) {
            prm->modeShowLoadPath = true;
            ++count;
        }
    }
    {   // OPTION_ROOT
        rc = ArgsOptionCount(args, OPTION_ROOT, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_ROOT "' argument");
            break;
        }
        if (pcount) {
            prm->modeRoot = true;
        }
    }
    {   // OPTION_SET
        rc = ArgsOptionCount(args, OPTION_SET, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_SET "' argument");
            break;
        }
        if (pcount) {
            rc = ArgsOptionValue(args, OPTION_SET, 0, &prm->setValue);
            if (rc == 0) {
                const char* p = strchr(prm->setValue, '=');
                if (p == NULL || *(p + 1) == '\0') {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR(klogErr, rc, "Bad " OPTION_SET " value");
                    break;
                }
                prm->modeSetNode = true;
                prm->modeCreate = prm->modeShowCfg = prm->modeShowEnv
                    = prm->modeShowFiles = prm->modeShowLoadPath
                    = prm->modeShowModules = false;
                count = 1;
            }
        }
    }
    {   // OPTION_FIX
        rc = ArgsOptionCount(args, OPTION_FIX, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_FIX "' argument");
            break;
        }
        if (pcount) {
            prm->modeConfigure = true;
            prm->modeShowCfg = false;
            count = 1;
            prm->configureMode = eCfgModeDefault;
        }
    }
    {   // OPTION_CFG
        rc = ArgsOptionCount(args, OPTION_CFG, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_CFG "' argument");
            break;
        }
        if (pcount) {
#if 1
            prm->modeConfigure = true;
            prm->modeShowCfg = false;
            count = 1;
            prm->configureMode = eCfgModeVisual;

#else
            const char* dummy = NULL;
            rc = ArgsOptionValue(args, OPTION_CFG, 0, &dummy);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_CFG "' argument");
                break;
            }
            prm->modeConfigure = true;
            prm->modeShowCfg = false;
            count = 1;
            switch (dummy[0]) {
                case 't':
                    prm->configureMode = eCfgModeTextual;
                    break;
                default:
                    prm->configureMode = eCfgModeDefault;
                    break;
            }
#endif
        }
    }
    {   // OPTION_CFM
        rc = ArgsOptionCount(args, OPTION_CFM, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_CFM "' argument");
            break;
        }
        if (pcount) {
            const char* dummy = NULL;
            size_t dummy_len;
            rc = ArgsOptionValue(args, OPTION_CFM, 0, &dummy);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_OUT "' argument");
                break;
            }
            prm->modeShowCfg = false;
            count = 1;
            prm->modeConfigure = true;

            dummy_len = strlen( dummy );
            if ( dummy_len == 0 )
                dummy_len = 1;

            if ( strncmp( dummy, "textual", dummy_len ) == 0 )
                prm->configureMode = eCfgModeTextual;
            else if ( strncmp( dummy, "graphical", dummy_len ) == 0 )
                prm->configureMode = eCfgModeVisual;
            else
            {
                rc = RC( rcExe, rcArgv, rcEvaluating, rcParam, rcInvalid );
                LOGERR(klogErr, rc, "Unrecognized '" OPTION_CFM "' argument");
                break;
            }
        }
    }
    {   // OPTION_ALL
        rc = ArgsOptionCount(args, OPTION_ALL, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_ALL "' argument");
            break;
        }
        if (pcount
            || ( !prm->modeConfigure
              && !prm->modeShowCfg && ! prm->modeShowLoadPath
              && !prm->modeShowEnv && !prm->modeShowFiles
              && !prm->modeShowModules && !prm->modeCreate
              && !prm->modeSetNode && !prm->ngc))
            /* show all by default */
        {
            prm->modeShowCfg = prm->modeShowEnv = prm->modeShowFiles = true;
            count += 2;
        }
    }

        if (count > 1)  {
            prm->showMultiple = true;
        }
    } while (false);

    return rc;
}

static rc_t ParamsGetNextParam(Params* prm, const char** param) {
    rc_t rc = 0;
    assert(prm && param);
    *param = NULL;
    if (prm->argsParamIdx < prm->argsParamCnt) {
        rc = ArgsParamValue(prm->args, prm->argsParamIdx++, param);
        if (rc)
        {   LOGERR(klogErr, rc, "Failure retrieving query"); }
    }
    return rc;
}

static rc_t ParamsDestruct(Params* prm) {
    rc_t rc = 0;
    assert(prm);
    RELEASE(Args, prm->args);
    return rc;
}

static
rc_t privReadStdinLine(char* buf, size_t bsize, bool destroy)
{
    rc_t rc = 0;
    static const KFile* std_in = NULL;
    static uint64_t pos = 0;
    size_t num_read = 0;
    if (destroy) {
        RELEASE(KFile, std_in);
        pos = 0;
        return rc;
    }
    if (std_in == NULL) {
        rc = KFileMakeStdIn(&std_in);
        if (rc != 0) {
            DISP_RC(rc, "KFileMakeStdIn");
            return rc;
        }
    }
    rc = KFileRead(std_in, pos, buf, bsize, &num_read);
    DISP_RC(rc, "KFileRead");
    pos += num_read;
    if (num_read) {
        bool done = false;
        buf[num_read] = '\0';
        while (num_read > 0 && !done) {
            switch (buf[num_read - 1]) {
                case '\n':
                case '\r':
                    buf[--num_read] = '\0';
                    break;
                default:
                    done = true;
                    break;
            }
        }
    }
    return rc;
}

static rc_t ReadStdinLine(char* buf, size_t bsize) {
    return privReadStdinLine(buf, bsize, false);
}

static rc_t DestroyStdin(void)
{   return privReadStdinLine(NULL, 0, true); }

static rc_t In(const char* prompt, const char* def, char** read) {
    rc_t rc = 0;
    char buf[PATH_MAX + 1];
    assert(prompt && read);
    *read = NULL;
    while (rc == 0 && (*read == NULL || read[0] == '\0')) {
        OUTMSG(("%s", prompt));
        if (def)
        {   OUTMSG((" [%s]", def)); }
        OUTMSG((": "));
        rc = ReadStdinLine(buf, sizeof buf);
        if (rc == 0) {
            while (strlen(buf) > 0) {
                char c = buf[strlen(buf) - 1];
                if (c == '\n' || c == '\r')
                {   buf[strlen(buf) - 1] = '\0'; }
                else
                {   break; }
            }
            if (buf[0] == '\0' && def != NULL) {
                string_copy_measure(buf, sizeof buf, def);
            }
            if (buf[0]) {
                *read = strdup(buf);
                if (*read == NULL) {
                    rc = RC
                        (rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                }
            }
        }
    }
    return rc;
}

static rc_t CC scan_config_dir(const KDirectory* dir,
    uint32_t type, const char* name, void* data)
{
    rc_t rc = 0;
    assert(data);
    switch (type) {
        case kptFile:
        case kptFile | kptAlias: {
            size_t sz = strlen(name);
            if (sz >= 5 && strcase_cmp(&name[sz - 4], 4, ".kfg", 4, 4) == 0) {
                string_copy_measure(data, PATH_MAX + 1, name);
                                  /* from CreateConfig..KDirectoryVVisit call */
                rc = RC(rcExe, rcDirectory, rcListing, rcFile, rcExists);
            }
            break;
        }
    }
    return rc;
}

static rc_t CreateConfig(char* argv0) {
    const KFile* std_in = NULL;
    KDirectory* native = NULL;
    KDirectory* dir = NULL;
    rc_t rc = 0;
    char* location = NULL;
    char* mod = NULL;
    char* wmod = NULL;
    char* refseq = NULL;
    if (rc == 0) {
        rc = KDirectoryNativeDir(&native);
    }
    if (rc == 0) {
        const char* def = NULL;
        char cwd[PATH_MAX + 1] = "";
        const char* home = getenv("HOME");
        if (home)
        {   def = home; }
        else {
            rc = KDirectoryResolvePath ( native, true, cwd, sizeof cwd, "." );
            if (rc == 0 && cwd[0])
            {   def = cwd; }
            else
            {   def = "."; }
        }
        while (rc == 0) {
            char buffer[PATH_MAX + 1];
            rc = In("Specify configuration files directory", def, &location);
            if (rc == 0) {
                rc = KDirectoryOpenDirUpdate(native, &dir, false, "%s", location);
                if (rc == 0) {
                    rc = KDirectoryVisit
                        (dir, false, scan_config_dir, buffer, ".");
                    if (rc != 0) {
                        if (rc ==
                             RC(rcExe, rcDirectory, rcListing, rcFile, rcExists)
                            && buffer[0])
                        {
                            PLOGERR(klogErr, (klogErr, rc,
                                "Configuration file found: $(dir)/$(name)",
                                "dir=%s,name=%s", location, buffer));
                            rc = 0;
                            buffer[0] = '\0';
                            continue;
                        }
                        else {
                            PLOGERR(klogErr, (klogErr, rc, "$(dir)/$(name)",
                                "dir=%s,name=%s", location, buffer));
                        }
                    }
                    break;
                }
                else if (GetRCObject(rc) == (enum RCObject)rcPath &&
                    (GetRCState(rc) == rcIncorrect || GetRCState(rc) == rcNotFound))
                {
                    PLOGERR(klogErr,
                        (klogErr, rc, "$(path)", "path=%s", location));
                    rc = 0;
                }
                else { DISP_RC(rc, location); }
            }
        }
    }
    while (rc == 0) {
        const KDirectory* dir = NULL;
        rc = In("Specify refseq installation directory", NULL, &refseq);
        if (rc != 0)
        {   break; }
        rc = KDirectoryOpenDirRead(native, &dir, false, "%s", refseq);
        if (rc == 0) {
            RELEASE(KDirectory, dir);
            break;
        }
        else if (GetRCObject(rc) == (enum RCObject)rcPath
              && GetRCState(rc) == rcIncorrect)
        {
            PLOGERR(klogErr,
                (klogErr, rc, "$(path)", "path=%s", refseq));
            rc = 0;
        }
        DISP_RC(rc, refseq);
    }
    if (rc == 0) {
        char buffer[512];
        const char path[] = "vdb-config.kfg";
        uint64_t pos = 0;
        KFile* f = NULL;
        rc = KDirectoryCreateFile(dir, &f, false, 0664, kcmCreate, "%s", path);
        DISP_RC(rc, path);
        if (rc == 0) {
            int n = snprintf(buffer, sizeof buffer,
                "refseq/servers = \"%s\"\n", refseq);
            if (n >= sizeof buffer) {
                rc = RC(rcExe, rcFile, rcWriting, rcBuffer, rcInsufficient);
            }
            else {
                size_t num_writ = 0;
                rc = KFileWrite(f, pos, buffer, strlen(buffer), &num_writ);
                pos += num_writ;
            }
        }
        if (rc == 0) {
            const char buffer[] = "refseq/volumes = \".\"\n";
            size_t num_writ = 0;
            rc = KFileWrite(f, pos, buffer, strlen(buffer), &num_writ);
            pos += num_writ;
        }
        if (rc == 0 && mod && mod[0]) {
            int n = snprintf(buffer, sizeof buffer,
                "vdb/module/paths = \"%s\"\n", mod);
            if (n >= sizeof buffer) {
                rc = RC(rcExe, rcFile, rcWriting, rcBuffer, rcInsufficient);
            }
            else {
                size_t num_writ = 0;
                rc = KFileWrite(f, pos, buffer, strlen(buffer), &num_writ);
                pos += num_writ;
            }
        }
        if (rc == 0 && wmod && wmod[0]) {
            int n = snprintf(buffer, sizeof buffer,
                "vdb/wmodule/paths = \"%s\"\n", wmod);
            if (n >= sizeof buffer) {
                rc = RC(rcExe, rcFile, rcWriting, rcBuffer, rcInsufficient);
            }
            else {
                size_t num_writ = 0;
                rc = KFileWrite(f, pos, buffer, strlen(buffer), &num_writ);
                pos += num_writ;
            }
        }
        RELEASE(KFile, f);
    }
    free(mod);
    free(wmod);
    free(refseq);
    free(location);
    RELEASE(KDirectory, dir);
    RELEASE(KDirectory, native);
    RELEASE(KFile, std_in);
    DestroyStdin();
    return rc;
}

#if 0
static rc_t ShowModules(const KConfig* cfg, const Params* prm) {
    rc_t rc = 0;
#ifdef _STATIC
    OUTMSG(("<!-- Modules are not used in static build -->\n"));
#else
    const VDBManager* mgr = NULL;
    KNamelist* list = NULL;
    OUTMSG(("<!-- Modules -->\n"));
    rc = VDBManagerMakeRead(&mgr, NULL);
    DISP_RC(rc, "while calling VDBManagerMakeRead");
    if (rc == 0) {
        rc = VDBManagerListExternalSchemaModules(mgr, &list);
        DISP_RC(rc, "while calling VDBManagerListExternalSchemaModules");
    }
    if (rc == 0) {
        uint32_t count = 0;
        rc = KNamelistCount(list, &count);
        DISP_RC(rc, "while calling KNamelistCount "
            "on VDBManagerListExternalSchemaModules result");
        if (rc == 0) {
            int64_t i = 0;
            for (i = 0; i < count && rc == 0; ++i) {
                const char* name = NULL;
                rc = KNamelistGet(list, i, &name);
                DISP_RC(rc, "while calling KNamelistGet "
                    "on VDBManagerListExternalSchemaModules result");
                if (rc == 0) {
                    OUTMSG(("%s\n", name));
                }
            }
        }
    }
    OUTMSG(("\n"));
    RELEASE(KNamelist, list);
    RELEASE(VDBManager, mgr);
#endif
    return rc;
}
#endif

static rc_t SetNode(KConfig* cfg, const Params* prm) {
    rc_t rc = 0;

    KConfigNode* node = NULL;
    char* name = NULL;
    char* val  = NULL;

    assert(cfg && prm && prm->setValue);

    if (is_iser_an_admin() && !prm->modeRoot) {
        rc = RC(rcExe, rcNode, rcUpdating, rcCondition, rcViolated);
        LOGERR(klogErr, rc, "Warning: "
            "normally this application should not be run as root/superuser");
    }

    name = strdup(prm->setValue);
    if (name == NULL)
    {   return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted); }

    val = strchr(name, '=');
    if (val == NULL || *(val + 1) == '\0') {
        rc_t rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
        LOGERR(klogErr, rc, "Bad " OPTION_SET " value");
    }

    if (rc == 0) {
        *(val++) = '\0';

        rc = KConfigOpenNodeUpdate(cfg, &node, "%s", name);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "Cannot open node '$(name)' for update", "name=%s", name));
        }
    }

    if (rc == 0) {
        assert(val);
        rc = KConfigNodeWrite(node, val, strlen(val));
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "Cannot write value '$(val) to node '$(name)'",
                "val=%s,name=%s", val, name));
        }
    }

    if (rc == 0) {
        rc = KConfigCommit(cfg);
        DISP_RC(rc, "while calling KConfigCommit");
    }

    free(name);
    name = NULL;

    RELEASE(KConfigNode, node);
    return rc;
}

static rc_t ShowConfig(const KConfig* cfg, Params* prm) {
    rc_t rc = 0;
    bool hasAny = false;
    bool hasQuery = false;
    bool xml = true;
    assert(cfg && prm);
    xml = prm->xml;
    while (rc == 0) {
        KNamelist* names = NULL;
        const KConfigNode* node = NULL;
        uint32_t count = 0;
        uint32_t i = 0;
        int indent = 0;
        const char* root = NULL;
        const char* nodeName = NULL;
        size_t nodeNameL = 1;
        rc = ParamsGetNextParam(prm, &root);
        if (rc == 0) {
            if (root == NULL) {
                if (hasQuery)
                {   break; }
                else
                {   root = "/"; }
            }
            else { hasQuery = true; }
            assert(root);
        }
        if (rc == 0) {
            int64_t len = strlen(root);
            assert(len > 0);
            while (len > 0) {
                if (root[len - 1] == '/')
                {   --len; }
                else { break; }
            }
            assert(len >= 0);
            if (len == 0) {
                root += strlen(root) - 1;
                nodeName = root;
            }
            else {
                char *c = memrchr(root, '/', len);
                if (c != NULL) {
                    nodeName = c + 1;
                }
                else {
                    nodeName = root;
                }
            }
            assert(nodeName && nodeName[0]);
            nodeNameL = strlen(nodeName);
            while (nodeNameL > 1 && nodeName[nodeNameL - 1] == '/')
            {   --nodeNameL; }
        }

        if (rc == 0) {
            rc = KConfigOpenNodeRead(cfg, &node, "%s", root);
            DISP_RC(rc, root);
        }
        if (rc == 0) {
            rc = KConfigNodeListChild(node, &names);
        }
        if (rc == 0) {
            rc = KNamelistCount(names, &count);
        }
        if (rc == 0 && count == 0) {
            char buf[512] = "";
            size_t num_read = 0;
            rc = KConfigNodeReadData(node, buf, sizeof buf, &num_read);
            if (rc == 0 && num_read > 0) {
                if (prm->showMultiple)
                {   OUTMSG(("<!-- Configuration node %s -->\n", root)); }
                if (xml) {
                    VDB_CONGIG_OUTMSG(("<%.*s>", nodeNameL, nodeName));
                    VDB_CONGIG_OUTMSG(("%.*s", (int)num_read, buf));
                    VDB_CONGIG_OUTMSG(("</%.*s>\n", nodeNameL, nodeName));
                }
                else {
                    OUTMSG(("%.*s = \"%.*s\"\n",
                        nodeNameL, nodeName, (int)num_read, buf));
                }
                hasAny = true;
            }
        }
        else {
            if (rc == 0) {
                if (nodeName[0] != '/') {
                    if (prm->showMultiple)
                    {   OUTMSG(("<!-- Configuration node %s -->\n", root)); }
                    VDB_CONGIG_OUTMSG(("<%.*s>\n", nodeNameL, nodeName));
                } else {
                    if (prm->showMultiple)
                    {   OUTMSG(("<!-- Current configuration -->\n")); }
                    VDB_CONGIG_OUTMSG(("<Config>\n"));
                }
                hasAny = true;
                ++indent;
            }
            for (i = 0; i < count && rc == 0; ++i) {
                const char* name = NULL;
                if (rc == 0)
                {   rc = KNamelistGet(names, i, &name); }
                if (rc == 0) {
                    char* fullname = NULL;
                    if (strcmp(root, "/") == 0) {
                        size_t bsize = strlen(name) + 2;
                        fullname = malloc(bsize);
                        if (fullname == NULL) {
                            rc = RC(rcExe,
                                rcStorage, rcAllocating, rcMemory, rcExhausted);
                        }
                        string_printf(fullname, bsize, NULL, "/%s", name);
                    }
                    else {
                        size_t sz = strlen(root) + 2 + strlen(name);
                        size_t num_writ = 0;
                        fullname = malloc(sz);
                        if (fullname == NULL) {
                            rc = RC(rcExe,
                                rcStorage, rcAllocating, rcMemory, rcExhausted);
                        }
                        rc = string_printf(fullname, sz, &num_writ,
                            "%s/%s", root, name);
                        assert(num_writ + 1 == sz);
                    }
                    if (rc == 0) {
                        rc = KConfigNodePrintChildNames
                            (xml, node, name, indent, fullname);
                        hasAny = true;
                    }
                    free(fullname);
                    fullname = NULL;
                }
            }
            if (rc == 0) {
                if (nodeName[0] != '/') {
                    VDB_CONGIG_OUTMSG(("</%.*s>\n", nodeNameL, nodeName));
                }
                else {
                    VDB_CONGIG_OUTMSG(("</Config>\n"));
                }
            }
        }

        RELEASE(KConfigNode, node);
        RELEASE(KNamelist, names);

        if (rc == 0) {
            if (hasAny) {
                OUTMSG(("\n"));
            }
            else if (nodeNameL > 0 && nodeName != NULL) {
                VDB_CONGIG_OUTMSG(("<%.*s/>\n", nodeNameL, nodeName));
            }
        }

        if (!hasQuery)
        {   break; }
    }

    return rc;
}

static rc_t ShowFiles(const KConfig* cfg, const Params* prm) {
    rc_t rc = 0;
    bool hasAny = false;
    uint32_t count = 0;
    KNamelist* names = NULL;
    rc = KConfigListIncluded(cfg, &names);
    if (rc == 0)
    {   rc = KNamelistCount(names, &count); }
    if (rc == 0) {
        uint32_t i = 0;

        if (prm->showMultiple) {
            OUTMSG(("<!-- Configuration files -->\n"));
            hasAny = true;
        }

        for (i = 0; i < count && rc == 0; ++i) {
            const char* name = NULL;
            if (rc == 0)
            {   rc = KNamelistGet(names, i, &name); }
            if (rc == 0) {
                OUTMSG(("%s\n", name));
                hasAny = true;
            }
        }
    }
    if (rc == 0 && hasAny)
    {   OUTMSG(("\n")); }

    RELEASE(KNamelist, names);

    return rc;
}

static void ShowEnv(const Params* prm) {
    bool hasAny = false;
    const char * env_list [] = {
        "KLIB_CONFIG",
        "VDB_CONFIG",
        "VDBCONFIG",
        "LD_LIBRARY_PATH"
    };
    int i = 0;

    if (prm->showMultiple) {
        OUTMSG(("<!-- Environment -->\n"));
        hasAny = true;
    }

    for (i = 0; i < sizeof env_list / sizeof env_list [ 0 ]; ++ i ) {
        const char *eval = getenv ( env_list [ i ] );
        if (eval) {
            OUTMSG(("%s=%s\n", env_list [ i ], eval));
            hasAny = true;
        }
    }
    if (hasAny) {
        OUTMSG(("\n"));
    }
    else {
        OUTMSG(("Environment variables are not found\n"));
    }
}

static rc_t _VFSManagerSystem2PosixPath(const VFSManager *self,
    const char *system, char posix[PATH_MAX])
{
    VPath *path = NULL;
    rc_t rc = VFSManagerMakeSysPath(self, &path, system);
    if (rc == 0) {
        size_t written;
        rc = VPathReadPath(path, posix, PATH_MAX, &written);
    }
    RELEASE(VPath, path);
    return rc;
}

static rc_t ImportNgc(KConfig *cfg, Params *prm, const char **newRepoParentPath)
{
    VFSManager *vmgr = NULL;
    rc_t rc = VFSManagerMake(&vmgr);
    char ngcPath[PATH_MAX] = "";
    KDirectory *dir = NULL;
    const KFile *src = NULL;
    const KNgcObj *ngc = NULL;
    static char buffer[PATH_MAX] = "";
    const char *root = NULL;
    KRepositoryMgr *rmgr = NULL;

    assert(prm);
    if (rc == 0) {
        rc = _VFSManagerSystem2PosixPath(vmgr, prm->ngc, ngcPath);
    }
    if (rc == 0) {
        rc = KDirectoryNativeDir(&dir);
    }
    if (rc == 0) {
        rc = KDirectoryOpenFileRead(dir, &src, "%s", ngcPath);
    }
    RELEASE(KDirectory, dir);
    if (rc == 0) {
        rc = KNgcObjMakeFromFile(&ngc, src);
    }
    RELEASE(KFile, src);
    if (rc == 0) {
        uint32_t id = 0;
        size_t written = 0;
        rc = KNgcObjGetProjectId(ngc, &id);
        if (rc == 0) {
            rc = ParamsGetNextParam(prm, &root);
            root = NULL; // TODO: we cannot accept repo default path from cmd ln
            if (rc != 0 || root == NULL) {
                char home[PATH_MAX] = "";
                rc = KConfig_Get_Default_User_Path(cfg,
                    home, sizeof home, &written);
                if (rc == 0 && written > 0) {
                    rc = string_printf(buffer, sizeof buffer,
                        &written, "%s/dbGaP-%u", home, id);
                    root = buffer;
                }
            }
            rc = 0;
            if (root == NULL && rc == 0) {
                char home[PATH_MAX] = "";
                rc = KConfig_Get_Home(cfg, home, sizeof home, &written);
                if (rc == 0 && written > 0) {
                    rc = string_printf(buffer, sizeof buffer,
                        &written, "%s/ncbi/dbGaP-%u", home, id);
                    root = buffer;
                }
            }
            if (root == NULL && rc == 0) {
                const char *home = getenv("HOME");
                if (home == NULL) {
                    home = getenv("USERPROFILE");
                }
                if (home == NULL) {
#define TODO 1
                    rc = TODO;
                }
                else {
                    size_t num_writ = 0;
                    char posix[PATH_MAX] = "";
                    rc = _VFSManagerSystem2PosixPath(vmgr, root, posix);
                    if (rc == 0) {
                        rc = string_printf(buffer, sizeof buffer,
                            &num_writ, "%s/ncbi/dbGaP-%u", posix, id);
                    }
                    if (rc == 0) {
                        root = buffer;
                    }
                }
            }
        }
    }
    RELEASE(VFSManager, vmgr);
    if (rc == 0) {
        rc = KConfigMakeRepositoryMgrUpdate(cfg, &rmgr);
    }
    if (rc == 0) {
        uint32_t result_flags = 0;
        assert(root);
        rc = KRepositoryMgrImportNgcObj(rmgr, ngc, root,
            INP_CREATE_REPOSITORY, &result_flags);
    }
    *newRepoParentPath = root;
    RELEASE(KNgcObj, ngc);
    RELEASE(KRepositoryMgr, rmgr);
    return rc;
}

rc_t CC KMain(int argc, char* argv[]) {
    rc_t rc = 0;

    Params prm;
    KConfig* cfg = NULL;

    if (rc == 0) {
        rc = ParamsConstruct(argc, argv, &prm);
    }

    if (rc == 0) {
        rc = KConfigMake(&cfg, NULL);
        DISP_RC(rc, "while calling KConfigMake");
    }

    if (rc == 0) {
        if (prm.modeConfigure) {
            rc = configure(prm.configureMode);
        }
        else if (prm.ngc) {
            const char *newRepoParentPath = NULL;
            rc = ImportNgc       (cfg,&prm          , &newRepoParentPath);
            DISP_RC2(rc, "cannot import ngc file", prm.ngc);
            if (rc == 0) {
                rc = KConfigCommit(cfg);
            }
            if (rc == 0) {
#if WINDOWS
                KDirectory *wd = NULL;
                char system[MAX_PATH] = "";
                rc_t rc = KDirectoryNativeDir(&wd);
                if (rc == 0) {
                    rc = KDirectoryPosixStringToSystemString(wd,
                        system, sizeof system, "%s", newRepoParentPath);
                    if (rc == 0) {
                        newRepoParentPath = system;
                    }
                }
#endif          
                OUTMSG((
                    "%s was imported\nThe new protected repository is: %s\n",
                    prm.ngc, newRepoParentPath));
            }
        }
        else if (prm.modeSetNode) {
            rc_t rc3 = SetNode(cfg, &prm);
            if (rc3 != 0 && rc == 0) {
                rc = rc3;
            }
        }
        if (prm.modeShowCfg) {
            rc_t rc3 = ShowConfig(cfg, &prm);
            if (rc3 != 0 && rc == 0) {
                rc = rc3;
            }
        }
        if (prm.modeShowFiles) {
            rc_t rc3 = ShowFiles(cfg, &prm);
            if (rc3 != 0 && rc == 0) {
                rc = rc3;
            }
        }
#if 0
        if (prm.modeShowModules) {
            rc_t rc3 = ShowModules(cfg, &prm);
            if (rc3 != 0 && rc == 0) {
                rc = rc3;
            }
        }
#endif
        if (prm.modeShowLoadPath) {
            const char* path = NULL;
            rc_t rc3 = KConfigGetLoadPath(cfg, &path);
            if (rc3 == 0) {
                if (path != NULL && path[0]) {
                    OUTMSG(("%s\n", path));
                }
            }
            else if (rc == 0) {
                rc = rc3;
            }
        }
    }

    if (prm.modeShowEnv) {
        ShowEnv(&prm);
    }

    RELEASE(KConfig, cfg);

    if (rc == 0 && prm.modeCreate)
    {   rc = CreateConfig(argv[0]); }

    ParamsDestruct(&prm);
    return rc;
}

/************************************* EOF ************************************/
