/****************************************************************************
Copyright (c) 2010-2013 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies Inc.

http://www.cocos2d-x.org

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
****************************************************************************/
#ifndef __CC_FILEUTILS_H__
#define __CC_FILEUTILS_H__

#include <string>
#include <vector>
#include <unordered_map>

#include "platform/CCPlatformMacros.h"
#include "base/ccTypes.h"
#include "base/CCValue.h"
#include "base/CCData.h"

NS_CC_BEGIN

/**
 * @addtogroup support
 * @{
 */

/** Helper class to handle file operations. */
class CC_DLL FileUtils
{
public:
    /**
     *  Gets the instance of FileUtils.
     */
    static FileUtils* getInstance();

    /**
     *  Destroys the instance of FileUtils.
     */
    static void destroyInstance();
    
    /**
     * You can inherit from platform dependent implementation of FileUtils, such as FileUtilsAndroid,
     * and use this function to set delegate, then FileUtils will invoke delegate's implementation.
     * Fox example, your resources are encrypted, so you need to decrypt it after reading data from 
     * resources, then you can implement all getXXX functions, and engine will invoke your own getXX
     * functions when reading data of resources.
     *
     * If you don't want to system default implementation after setting delegate, you can just pass nullptr
     * to this function.
     *
     * @warning It will delete previous delegate
     * @lua NA
     */
    static void setDelegate(FileUtils *delegate);

    /** @deprecated Use getInstance() instead */
    CC_DEPRECATED_ATTRIBUTE static FileUtils* sharedFileUtils() { return getInstance(); }

    /** @deprecated Use destroyInstance() instead */
    CC_DEPRECATED_ATTRIBUTE static void purgeFileUtils() { destroyInstance(); }

    /**
     *  The destructor of FileUtils.
     * @js NA
     * @lua NA
     */
    virtual ~FileUtils();
    
    /**
     *  Purges full path caches.
     */
    virtual void purgeCachedEntries();
    
    /**
     *  Gets string from a file.
     */
    virtual std::string getStringFromFile(const std::string& filename);
    
    /**
     *  Creates binary data from a file.
     *  @return A data object.
     */
    virtual Data getDataFromFile(const std::string& filename);
    
    /**
     *  Gets resource file data
     *
     *  @param[in]  filename The resource file name which contains the path.
     *  @param[in]  mode The read mode of the file.
     *  @param[out] size If the file read operation succeeds, it will be the data size, otherwise 0.
     *  @return Upon success, a pointer to the data is returned, otherwise NULL.
     *  @warning Recall: you are responsible for calling free() on any Non-NULL pointer returned.
     */
    CC_DEPRECATED_ATTRIBUTE virtual unsigned char* getFileData(const std::string& filename, const char* mode, ssize_t *size);

    /**
     *  Gets resource file data from a zip file.
     *
     *  @param[in]  filename The resource file name which contains the relative path of the zip file.
     *  @param[out] size If the file read operation succeeds, it will be the data size, otherwise 0.
     *  @return Upon success, a pointer to the data is returned, otherwise nullptr.
     *  @warning Recall: you are responsible for calling free() on any Non-nullptr pointer returned.
     */
    virtual unsigned char* getFileDataFromZip(const std::string& zipFilePath, const std::string& filename, ssize_t *size);

    
    /** Returns the fullpath for a given filename.
     根据一个文件名返回一个绝对路径.
     First it will try to get a new filename from the "filenameLookup" dictionary.
     If a new filename can't be found on the dictionary, it will use the original filename.
     第一步，它会在“filenamelookup”字典里找到一个新的文件名(通过key-value对查找)
     如果这个新的文件名无法找到，那么他就用原来的文件名.

     Then it will try to obtain the full path of the filename using the FileUtils search rules: resolutions, and search paths.
     The file search is based on the array element order of search paths and resolution directories.
     第二步，它会根据原来设定的搜索路径，将filename添加到相对路径上去
     For instance:

     	We set two elements("/mnt/sdcard/", "internal_dir/") to search paths vector by setSearchPaths,
     	and set three elements("resources-ipadhd/", "resources-ipad/", "resources-iphonehd")
     	to resolutions vector by setSearchResolutionsOrder. The "internal_dir" is relative to "Resources/".

		If we have a file named 'sprite.png', the mapping in fileLookup dictionary contains `key: sprite.png -> value: sprite.pvr.gz`.
     	Firstly, it will replace 'sprite.png' with 'sprite.pvr.gz', then searching the file sprite.pvr.gz as follows:


        假设我们通过setSearchPaths函数往_searchPathArray添加了两个元素(“/mnt/sdcard/”, “internal_dir/”)，
        还通过setSearchResolutionsOrder函数往_searchResolutionsOrderArray添加了三个元素
        (“resources-ipadhd/”, “resources-ipad/”, “resources-iphonehd”)。对于给定一张名叫“sprite.png”的图片，
        在_filenameLookupDict中存在这样一个键值对：key: sprite.png -> value: sprite.pvr.gz，
        Cocos2d-x就会按照下面的顺序查找：

     	    /mnt/sdcard/resources-ipadhd/sprite.pvr.gz      (if not found, search next)
     	    /mnt/sdcard/resources-ipad/sprite.pvr.gz        (if not found, search next)
     	    /mnt/sdcard/resources-iphonehd/sprite.pvr.gz    (if not found, search next)
     	    /mnt/sdcard/sprite.pvr.gz                       (if not found, search next)
     	    internal_dir/resources-ipadhd/sprite.pvr.gz     (if not found, search next)
     	    internal_dir/resources-ipad/sprite.pvr.gz       (if not found, search next)
     	    internal_dir/resources-iphonehd/sprite.pvr.gz   (if not found, search next)
     	    internal_dir/sprite.pvr.gz                      (if not found, return "sprite.png")
        





        If the filename contains relative path like "gamescene/uilayer/sprite.png",
        and the mapping in fileLookup dictionary contains `key: gamescene/uilayer/sprite.png -> value: gamescene/uilayer/sprite.pvr.gz`.
        The file search order will be:

     	    /mnt/sdcard/gamescene/uilayer/resources-ipadhd/sprite.pvr.gz      (if not found, search next)
     	    /mnt/sdcard/gamescene/uilayer/resources-ipad/sprite.pvr.gz        (if not found, search next)
     	    /mnt/sdcard/gamescene/uilayer/resources-iphonehd/sprite.pvr.gz    (if not found, search next)
     	    /mnt/sdcard/gamescene/uilayer/sprite.pvr.gz                       (if not found, search next)
     	    internal_dir/gamescene/uilayer/resources-ipadhd/sprite.pvr.gz     (if not found, search next)
     	    internal_dir/gamescene/uilayer/resources-ipad/sprite.pvr.gz       (if not found, search next)
     	    internal_dir/gamescene/uilayer/resources-iphonehd/sprite.pvr.gz   (if not found, search next)
     	    internal_dir/gamescene/uilayer/sprite.pvr.gz                      (if not found, return "gamescene/uilayer/sprite.png")

     If the new file can't be found on the file system, it will return the parameter filename directly.
     
     This method was added to simplify multiplatform support. Whether you are using cocos2d-js or any cross-compilation toolchain like StellaSDK or Apportable,
     you might need to load different resources for a given file in the different platforms.

     @since v2.1
     */
    /*
      通过文件名称来查找文件的全路径，来实现文件的跨平台查找。

      路径测试：
      std::string fullPath = FileUtils::getInstance()->fullPathForFilename("HelloWorld.png");
      log("%s", fullPath.c_str());
      
      输出-->D:/cocos2d-x-3.8/projects/CocosTest/proj.win32/Debug.win32/HelloWorld.png.是Visual Studio项目的调试Debug目录，
      而HelloWorld.png则是我们指定的文件名称。 
    */
    virtual std::string fullPathForFilename(const std::string &filename) const;
    
    /**
     * Loads the filenameLookup dictionary from the contents of a filename.
     * 
     * @note The plist file name should follow the format below:
     * 
     * @code
     * <?xml version="1.0" encoding="UTF-8"?>
     * <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
     * <plist version="1.0">
     * <dict>
     *     <key>filenames</key>
     *     <dict>
     *         <key>sounds/click.wav</key>
     *         <string>sounds/click.caf</string>
     *         <key>sounds/endgame.wav</key>
     *         <string>sounds/endgame.caf</string>
     *         <key>sounds/gem-0.wav</key>
     *         <string>sounds/gem-0.caf</string>
     *     </dict>
     *     <key>metadata</key>
     *     <dict>
     *         <key>version</key>
     *         <integer>1</integer>
     *     </dict>
     * </dict>
     * </plist>
     * @endcode
     * @param filename The plist file name.
     *
     @since v2.1
     * @js loadFilenameLookup
     * @lua loadFilenameLookup
     */
    virtual void loadFilenameLookupDictionaryFromFile(const std::string &filename);
    
    /** 
     *  Sets the filenameLookup dictionary.
     *
     *  @param pFilenameLookupDict The dictionary for replacing filename.
     *  @since v2.1
     */
    virtual void setFilenameLookupDictionary(const ValueMap& filenameLookupDict);
    
    /**
     *  Gets full path from a file name and the path of the relative file.
     *  @param filename The file name.
     *  @param pszRelativeFile The path of the relative file.
     *  @return The full path.
     *          e.g. filename: hello.png, pszRelativeFile: /User/path1/path2/hello.plist
     *               Return: /User/path1/path2/hello.pvr (If there a a key(hello.png)-value(hello.pvr) in FilenameLookup dictionary. )
     *
     */
    virtual std::string fullPathFromRelativeFile(const std::string &filename, const std::string &relativeFile);

    /** 
     *  Sets the array that contains the search order of the resources.
     *
     *  @param searchResolutionsOrder The source array that contains the search order of the resources.
     *  @see getSearchResolutionsOrder(), fullPathForFilename(const char*).
     *  @since v2.1
     *  In js:var setSearchResolutionsOrder(var jsval)
     *  @lua NA
     */
    virtual void setSearchResolutionsOrder(const std::vector<std::string>& searchResolutionsOrder);

    /**
      * Append search order of the resources.
      *
      * @see setSearchResolutionsOrder(), fullPathForFilename().
      * @since v2.1
      */
    virtual void addSearchResolutionsOrder(const std::string &order,const bool front=false);
    
    /**
     *  Gets the array that contains the search order of the resources.
     *
     *  @see setSearchResolutionsOrder(const std::vector<std::string>&), fullPathForFilename(const char*).
     *  @since v2.1
     *  @lua NA
     */
    virtual const std::vector<std::string>& getSearchResolutionsOrder() const;
    
    /** 
     *  Sets the array of search paths.
     * 
     *  You can use this array to modify the search path of the resources.
     *  If you want to use "themes" or search resources in the "cache", you can do it easily by adding new entries in this array.
     *
     *  @note This method could access relative path and absolute path.
     *        If the relative path was passed to the vector, FileUtils will add the default resource directory before the relative path.
     *        For instance:
     *        	On Android, the default resource root path is "assets/".
     *        	If "/mnt/sdcard/" and "resources-large" were set to the search paths vector,
     *        	"resources-large" will be converted to "assets/resources-large" since it was a relative path.
     *
     *  @param searchPaths The array contains search paths.
     *  @see fullPathForFilename(const char*)
     *  @since v2.1
     *  In js:var setSearchPaths(var jsval);
     *  @lua NA
     */
    virtual void setSearchPaths(const std::vector<std::string>& searchPaths);
    
    /**
     * Set default resource root path.
     */
    void setDefaultResourceRootPath(const std::string& path);

    /**
      * Add search path.
      *
      * @since v2.1
      */
    void addSearchPath(const std::string & path, const bool front=false);
    
    /**
     *  Gets the array of search paths.
     *  
     *  @return The array of search paths.
     *  @see fullPathForFilename(const char*).
     *  @lua NA
     */
    virtual const std::vector<std::string>& getSearchPaths() const;

    /**
     *  Gets the writable path.
     *  @return  The path that can be write/read a file in
     */
    virtual std::string getWritablePath() const = 0;
    
    /**
     *  Sets writable path.
     */
    virtual void setWritablePath(const std::string& writablePath);

    /**
     *  Sets whether to pop-up a message box when failed to load an image.
     */
    virtual void setPopupNotify(bool notify);
    
    /** Checks whether to pop up a message box when failed to load an image. 
     *  @return True if pop up a message box when failed to load an image, false if not.
     */
    virtual bool isPopupNotify() const;

    /**
     *  Converts the contents of a file to a ValueMap.
     *  @param filename The filename of the file to gets content.
     *  @return ValueMap of the file contents.
     *  @note This method is used internally.
     */
    virtual ValueMap getValueMapFromFile(const std::string& filename);

    // Converts the contents of a file to a ValueMap.
    // This method is used internally.
    virtual ValueMap getValueMapFromData(const char* filedata, int filesize);
    

    // Write a ValueMap to a plist file.
    // This method is used internally.
    virtual bool writeToFile(ValueMap& dict, const std::string& fullPath);
    
    // Converts the contents of a file to a ValueVector.
    // This method is used internally.
    virtual ValueVector getValueVectorFromFile(const std::string& filename);
    
    /**
     *  Checks whether a file exists.
     *
     *  @note If a relative path was passed in, it will be inserted a default root path at the beginning.
     *  @param filename The path of the file, it could be a relative or absolute path.
     *  @return True if the file exists, false if not.
     */
    virtual bool isFileExist(const std::string& filename) const;
    
    /**
     *  Checks whether the path is an absolute path.
     *
     *  @note On Android, if the parameter passed in is relative to "assets/", this method will treat it as an absolute path.
     *        Also on Blackberry, path starts with "app/native/Resources/" is treated as an absolute path.
     *
     *  @param path The path that needs to be checked.
     *  @return True if it's an absolute path, false if not.
     */
    virtual bool isAbsolutePath(const std::string& path) const;
    
    /**
     *  Checks whether the path is a directory.
     *
     *  @param dirPath The path of the directory, it could be a relative or an absolute path.
     *  @return True if the directory exists, false if not.
     */
    virtual bool isDirectoryExist(const std::string& dirPath) const;
    
    /**
     *  Creates a directory.
     *
     *  @param dirPath The path of the directory, it must be an absolute path.
     *  @return True if the directory have been created successfully, false if not.
     */
    virtual bool createDirectory(const std::string& dirPath);
    
    /**
     *  Removes a directory.
     *
     *  @param dirPath  The full path of the directory, it must be an absolute path.
     *  @return True if the directory have been removed successfully, false if not.
     */
    virtual bool removeDirectory(const std::string& dirPath);
    
    /**
     *  Removes a file.
     *
     *  @param filepath The full path of the file, it must be an absolute path.
     *  @return True if the file have been removed successfully, false if not.
     */
    virtual bool removeFile(const std::string &filepath);
    
    /**
     *  Renames a file under the given directory.
     *
     *  @param path     The parent directory path of the file, it must be an absolute path.
     *  @param oldname  The current name of the file.
     *  @param name     The new name of the file.
     *  @return True if the file have been renamed successfully, false if not.
     */
    virtual bool renameFile(const std::string &path, const std::string &oldname, const std::string &name);
    
    /**
     *  Retrieve the file size.
     *
     *  @note If a relative path was passed in, it will be inserted a default root path at the beginning.
     *  @param filepath The path of the file, it could be a relative or absolute path.
     *  @return The file size.
     */
    virtual long getFileSize(const std::string &filepath);

    /** Returns the full path cache. */
    const std::unordered_map<std::string, std::string>& getFullPathCache() const { return _fullPathCache; }

protected:
    /**
     *  The default constructor.
     */
    FileUtils();
    
    /**
     *  Initializes the instance of FileUtils. It will set _searchPathArray and _searchResolutionsOrderArray to default values.
     *
     *  @note When you are porting Cocos2d-x to a new platform, you may need to take care of this method.
     *        You could assign a default value to _defaultResRootPath in the subclass of FileUtils(e.g. FileUtilsAndroid). Then invoke the FileUtils::init().
     *  @return true if successed, otherwise it returns false.

     父类FileUtils的init方法里，会通过_defaultResRootPath去设置_searchPathArray和_searchResolutionsOrderArray的默认值，
     并且_defaultResRootPath会在不同的平台下，由不同的子类进行初始化设置，这样一来就形成了跨平台环境下资源路径的正确
     初始化以及后续的查找逻辑。

     
     *
     */
    virtual bool init();
    
    /**
     *  Gets the new filename from the filename lookup dictionary.
     *  It is possible to have a override names.
     *  @param filename The original filename.
     *  @return The new filename after searching in the filename lookup dictionary.
     *          If the original filename wasn't in the dictionary, it will return the original filename.
     */
    virtual std::string getNewFilename(const std::string &filename) const;
    
    /**
     *  Checks whether a file exists without considering search paths and resolution orders.
     *  @param filename The file (with absolute path) to look up for
     *  @return Returns true if the file found at the given absolute path, otherwise returns false
     */
    virtual bool isFileExistInternal(const std::string& filename) const = 0;
    
    /**
     *  Checks whether a directory exists without considering search paths and resolution orders.
     *  @param dirPath The directory (with absolute path) to look up for
     *  @return Returns true if the directory found at the given absolute path, otherwise returns false
     */
    virtual bool isDirectoryExistInternal(const std::string& dirPath) const;
    
    /**
     *  Gets full path for filename, resolution directory and search path.
     *
     *  @param filename The file name.
     *  @param resolutionDirectory The resolution directory.
     *  @param searchPath The search path.
     *  @return The full path of the file. It will return an empty string if the full path of the file doesn't exist.
     */
    virtual std::string getPathForFilename(const std::string& filename, const std::string& resolutionDirectory, const std::string& searchPath) const;
    
    /**
     *  Gets full path for the directory and the filename.
     *
     *  @note Only iOS and Mac need to override this method since they are using
     *        `[[NSBundle mainBundle] pathForResource: ofType: inDirectory:]` to make a full path.
     *        Other platforms will use the default implementation of this method.
     *  @param directory The directory contains the file we are looking for.
     *  @param filename  The name of the file.
     *  @return The full path of the file, if the file can't be found, it will return an empty string.
     */
    virtual std::string getFullPathForDirectoryAndFilename(const std::string& directory, const std::string& filename) const;
    
    /** 
     *  Returns the fullpath for a given filename.
     *  This is an alternative for fullPathForFilename
     *  It returns empty string instead of the original filename when no file found for the given name.
     *  @param filename The file name to look up for
     *  @return The full path for the file, if not found, the return value will be an empty string
     */
    virtual std::string searchFullPathForFilename(const std::string& filename) const;
    
    
    /** Dictionary used to lookup filenames based on a key.
     *  It is used internally by the following methods:
     *
     *  std::string fullPathForFilename(const char*);
     *
     *  @since v2.1
     */
    ValueMap _filenameLookupDict;
    
    /** 
     *  The vector contains resolution folders.
     *  The lower index of the element in this vector, the higher priority for this resolution directory.
     
     _searchResolutionsOrderArray是包含“分辨率文件夹”的列表，并且也是索引值越小，优先级越高。
     一般游戏都需要适配不同分辨率的手机屏幕，_searchResolutionsOrderArray
     把不同分辨率的图片资源组织到不同的文件夹下，方便游戏的开发。
     */
    std::vector<std::string> _searchResolutionsOrderArray;
    
    /**
     * The vector contains search paths.
     * The lower index of the element in this vector, the higher priority for this search path.

     _searchPathArray代表一个查找路径的列表，而且索引值越小，优先级越高。
     代码测试：
      std::vector<std::string> searchPaths = FileUtils::getInstance()->getSearchPaths();
      for (int i = 0; i < searchPaths.size(); i++)
        log("\t%d => %s", i, searchPaths.at(i).c_str());
     输出：
       0=>,D:/cocos2d-x-3.8/projects/CocosTest/proj.win32/Debug.win32

     _searchPathArray中包含一个元素，指向Win32程序的Debug目录。
     （值得注意的是：我们虽然把图片放在Resources目录下，但是，当发布到不同平台的时候，
     Cocos2d-x会把相应的文件拷贝到平台对应的目录下，Class目录也是这样）。由于我们没有适配不同分辨率的设备，
     _searchResolutionsOrderArray默认只有一个空字符串。


     _searchPathArray表示不同平台下放置图片资源的根查找目录，这里，我们不妨称它为“资源根目录”。而_searchResolutionsOrderArray对应相关分辨率目录，结合图片名称filename，
     Cocos2d-x构造了这样的文件路径：
     _searchPathArray[i] + _searchResolutionsOrderArray[j] + filename

     */
    std::vector<std::string> _searchPathArray;
    
    /**
     *  The default root path of resources.
     *  If the default root path of resources needs to be changed, do it in the `init` method of FileUtils's subclass.
     *  For instance:
     *  On Android, the default root path of resources will be assigned with "assets/" in FileUtilsAndroid::init().
     *  Similarly on Blackberry, we assign "app/native/Resources/" to this variable in FileUtilsBlackberry::init().
     */
    std::string _defaultResRootPath;
    
    /**
     *  The full path cache. When a file is found, it will be added into this cache. 
     *  This variable is used for improving the performance of file search.
     文件路径的缓存，当文件被找到时会添加到这个缓存中，下次再使用到这个文件时就直接从
     _fullPathCache返回该文件的全路径，从而提高游戏的运行效率。
     */
    mutable std::unordered_map<std::string, std::string> _fullPathCache;
    
    /**
     * Writable path.
     */
    std::string _writablePath;

    /**
     *  The singleton pointer of FileUtils.
     1，为了实现跨平台，引擎先抽象出一个父类FileUtils，并定义了s_sharedFileUtils 单例对象。
     2，每一个不同的平台都对应了一个名叫FileUtils-xxx的子类，s_sharedFileUtils指向的是该子类的实例对象。
     在FileUtils-xxx的init函数中，产生该平台对应的资源路径，并添加到_searchPathArray变量中。
     3，FileUtils-xxx还根据目标平台的特点重写FileUtils函数中平台相关的文件操作函数。
     这些函数在FileUtils中被定义为virtual function，根据多态的原理，会调用FileUtils-xxx子类的对应函数。
     4，如何实现不同平台下调用不同的FileUtil的呢？
     答案在：CCApplication.h里，请转到-->CCApplication.h文件中。
     */
    static FileUtils* s_sharedFileUtils;
    
};

// end of support group
/** @} */

NS_CC_END

#endif    // __CC_FILEUTILS_H__
