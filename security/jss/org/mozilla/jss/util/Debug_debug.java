/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/**********************************************************************
 * --------------------------- W A R N I N G --------------------------
 *
 * This file is the same as Debug_ship.java, except the static final
 * constants have been set to enable debugging and tracing.  You must
 * double-edit any changes in this file into Debug_ship.java, and 
 * vice-versa.
 * See debug_tweak.mk for how one of these two files is copied into
 * Debug.java during the build.
 **********************************************************************/

package com.netscape.jss.util;

/**
 * Controls debug-mode operation of JSS classes, and allows for printing
 * trace statements to standard output.
 *
 * @see com.netscape.jss.util.Assert
 * @version $Revision: 1.1 $ $Date: 2000/12/15 20:52:39 $
 */
public class Debug
{
    /**
     * Controls debug mode for JSS.  If DEBUG==true, debugging
     * code will be enabled.  If DEBUG==false, debugging code will not be
     * executed.  This variable does not influence the printing of trace
     * statements; their execution depends on the debug level, which can
     * be accessed through setLevel and getLevel.
     *
     * @see com.netscape.jss.util.Assert
     */
    public static final boolean DEBUG = true;

    public static final int OBNOXIOUS = 10;
    public static final int VERBOSE = 5;
    public static final int ERROR = 1;
    public static final int QUIET = 0;

    /**
     * The debug level of the application. This gives the level of detail
     * trace messages will contain.  A level of 0 means no debugging 
     * statements will be printed.
     *
     * !!If you change this, change it in the native code too!!
     */
	private static int mDebugLevel = VERBOSE;

    /**
     * Print a trace statement to standard output.
     *
     * @param level The detail level of the statement.
     *  The level must be greater than 0.
     * @param str The trace statement.
     */
    public synchronized static void trace(int level, String str)
    {
        // validate arguments in debug mode
        if(DEBUG && (level < 0) ) {
            throw new AssertionException("invalid debugging level "+level+
                " in trace");
        }
		if (level <= mDebugLevel)
		{
			System.out.println(Thread.currentThread().getName() + ": " + str);
			System.out.flush();
		}
    }

    /**
     * Print a trace statement to standard output.
     * Uses the VERBOSE detail level.
     *
     * @param str The trace statement.
     */
    public synchronized static void trace(String str)
	{
            trace(VERBOSE, str);
	}

    /**
     * Set the debugging level of the application.
     * The level must not be negative.
     */
    public synchronized static void setLevel(int level)
	{
        // In debugging mode, validate argument
        if( DEBUG && (level < 0) ) {
            throw new AssertionException("invalid debugging level set");
        }

		mDebugLevel = level;
        setNativeLevel(level);
	}
    private static native void setNativeLevel(int level);

    /**
     * Get debugging level of the application.
     *
     * @return The current debugging level of the application.
     */
    public synchronized static int getLevel() {
		return mDebugLevel;
	}

    public synchronized static String getLevelStr() {
        switch(mDebugLevel) {
        case QUIET: return "QUIET";
        case ERROR: return "ERROR";
        case VERBOSE: return "VERBOSE";
        case OBNOXIOUS: return "OBNOXIOUS";
        default:
            return String.valueOf(mDebugLevel);
        }
    }
}

