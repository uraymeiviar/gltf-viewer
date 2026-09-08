
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include "helpers/QtFileSystem.h"
#include "core/Slice.h"
#include "core/Platform.h"
#include <QDir>
#include <QUrl>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <condition_variable>
#include <thread>
#include <QStandardPaths>

std::condition_variable cv;
std::mutex cv_m; // This mutex is used for three purposes:
                 // 1) to synchronize accesses to i
                 // 2) to synchronize accesses to std::cerr
                 // 3) for the condition variable cv
int i = 0;

using namespace ux3d;
using namespace ux3d::editor;
using namespace ux3d::filesystem;

void sandboxSaveBookmarkFromNSUrl(const NSURL* url)
{
    NSData *bookmark = nil;
    NSError *error = nil;
    bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
             includingResourceValuesForKeys:nil
                              relativeToURL:nil // Make it app-scoped
                                      error:&error];
    if (error) {
        NSLog(@"Error while creating bookmark for URL (%@): %@", url, error);
    }
    NSString *access = [NSString stringWithFormat:@"%@%@", @"Access:", [url path]];
    [[NSUserDefaults standardUserDefaults] setObject:bookmark forKey:access];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

bool IFileSystem::sandboxIsExempt(const filesystem::Path& file)
{
#ifdef UX3D_APP_SANDBOX
    if (QtFileSystem::isQrc(file)) {
        return true;
    }
    
    QString qsPath = QString::fromStdString(file.toString());
    QString sandboxContainerPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (qsPath.startsWith(sandboxContainerPath)) {
        return true;
    }
    
    QString tmpPath = QDir::tempPath();
    if (qsPath.startsWith(tmpPath)) {
        return true;
    }
    
    
    return false;
#endif
    return true;
}

/**
 save URL as a bookmark so that it can later be loaded without a sandbox error
 https://stackoverflow.com/questions/54239776/macos-sandboxed-application-access-files-without-nsopenpanel
 */
void IFileSystem::sandboxSaveBookmark(const filesystem::File& file)
{
#ifdef UX3D_APP_SANDBOX
    if (sandboxIsExempt(file)) {
        return;
    }
    QUrl url = QtFileSystem::getQUrl(file);
    NSURL* selectedURL = url.toNSURL();
    sandboxSaveBookmarkFromNSUrl(selectedURL);
#endif
}

bool IFileSystem::sandboxRequestAccess(const filesystem::Path& path)
{
#ifdef UX3D_APP_SANDBOX
    if (!QtFileSystem::sandboxHasBookmark(path)) {
        QMessageBox msgBox;
        msgBox.setText(QString("Gestaltor needs permission to access \"%1\".").arg(QString::fromStdString(path.toString())));
        msgBox.setInformativeText("In the following you can give Gestaltor access to the files in the selected folder");
        msgBox.addButton("Select...", QMessageBox::AcceptRole);
        auto cancelButton = msgBox.addButton("Cancel", QMessageBox::RejectRole);
        msgBox.exec();
        if (msgBox.clickedButton() == cancelButton) {
            return false;
        }
        
        QFileDialog directoryDialog(nullptr, "Grant access to a directory", QString::fromStdString(path.toString()));
        directoryDialog.setLabelText(QFileDialog::Accept, "Allow Access");
        directoryDialog.setFileMode(QFileDialog::Directory);
        if(directoryDialog.exec() != QDialog::Accepted || directoryDialog.selectedFiles().empty()) {
            return false;
        }
        
        for (auto fileName : directoryDialog.selectedFiles()) {
            filesystem::Path bookmarkPath(QtFileSystem::instance(), fileName.toStdString());
            QtFileSystem::sandboxSaveBookmark(bookmarkPath);
        }
    }
#endif
    
    return true;
}

NSURL* findBookmark(filesystem::Path path)
{
    filesystem::Path parentPath = path.getParent();
    
    // TODO: extract that search to a separate function, so that it can also be called in the release access function
    while(path != parentPath) {
        QUrl url = QtFileSystem::getQUrl(path);
        NSURL* fileURL = url.toNSURL();
        NSString *accessKey = [NSString stringWithFormat:@"%@%@", @"Access:", [fileURL path]];
        NSData *bookmarkData = [[NSUserDefaults standardUserDefaults] objectForKey:accessKey];
        if (bookmarkData == nil){
            path = parentPath;
            parentPath = parentPath.getParent();
            continue;
        } else {
            NSError *error = nil;
            BOOL bookmarkDataIsStale;

            return [NSURL
                               URLByResolvingBookmarkData:bookmarkData
                               options:NSURLBookmarkResolutionWithSecurityScope
                               relativeToURL:nil
                               bookmarkDataIsStale:&bookmarkDataIsStale
                               error:&error];
        }
    }
    
    return nil;
}

bool IFileSystem::sandboxHasBookmark(const filesystem::File& file)
{
#ifdef UX3D_APP_SANDBOX
    if (sandboxIsExempt(file)) {
        return true;
    }
    
    NSURL* bookmarkFileURL = findBookmark(file);
    if (bookmarkFileURL == nil) {
        return false;
    }
#endif
    
    return true;
}

bool IFileSystem::sandboxGetAccessWithBookmark(const filesystem::File& file)
{
#ifdef UX3D_APP_SANDBOX
    if (sandboxIsExempt(file)) {
        return true;
    }
    
    NSURL* bookmarkFileURL = findBookmark(file);
    if (bookmarkFileURL == nil) {
        return false;
    }
    
    if ([bookmarkFileURL startAccessingSecurityScopedResource] == false) {
        return false;
    }
#endif
    
    return true;
}

void IFileSystem::sandboxReleaseAccessWithBookmark(const filesystem::File& file)
{
#ifdef UX3D_APP_SANDBOX
    if (sandboxIsExempt(file)) {
        return;
    }
    
    NSURL* bookmarkFileURL = findBookmark(file);
    if (bookmarkFileURL == nil) {
        return;
    }
    
    [bookmarkFileURL stopAccessingSecurityScopedResource];
#endif
}
