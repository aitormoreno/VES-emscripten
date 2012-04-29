/*========================================================================
  VES --- VTK OpenGL ES Rendering Toolkit

      http://www.kitware.com/ves

  Copyright 2011 Kitware, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 ========================================================================*/

#import "kiwiAppDelegate.h"
#import "GLViewController.h"
#import "EAGLView.h"
#import "InfoView.h"
#import "TitleBarViewContainer.h"

#include "vesKiwiViewerApp.h"

@implementation kiwiAppDelegate

@synthesize window;
@synthesize glView;
@synthesize viewController;
@synthesize dataLoader = _dataLoader;
@synthesize loadDataPopover = _loadDataPopover;
@synthesize lastSessionId;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions   
{
  self.window.rootViewController = self.viewController;
	NSURL *url = (NSURL *)[launchOptions valueForKey:UIApplicationLaunchOptionsURLKey];
	[self handleCustomURLScheme:url];
  return YES;
}

- (void)dealloc
{
  self.loadDataPopover = nil;

  [window release];
  [glView release];
  [viewController release];
  [_dataLoader release];
  [super dealloc];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

-(IBAction)reset:(UIButton*)sender
{
  [glView resetView];
}


-(IBAction)information:(UIButton*)sender
{
  InfoView *infoView = [[[InfoView alloc] initWithFrame:CGRectMake(0,0,320,260)] autorelease];

  if(UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
    {
    TitleBarViewContainer* container = [TitleBarViewContainer new];
    infoView.contentView.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.0];
    [container addViewToContainer:infoView];
    container.previousViewController = self.window.rootViewController;    
    [self.viewController presentModalViewController:container animated:YES];
    }
  else
    {
    UIViewController* newController = [UIViewController new];
    newController.view = infoView;
    UIPopoverController *popover = [[UIPopoverController alloc] initWithContentViewController:newController];
    [popover setPopoverContentSize:CGSizeMake(320,260) animated:NO];
    [popover presentPopoverFromRect:[(UIButton*)sender frame] inView:self.glView 
           permittedArrowDirections:(UIPopoverArrowDirectionDown)
                           animated:NO];
                           
    self.viewController.infoPopover = popover;
    }
  // need to get the info from the renderer
  [infoView updateModelInfoLabelWithNumFacets:[self.glView getNumberOfFacetsForCurrentModel]
                                 withNumLines:[self.glView getNumberOfLinesForCurrentModel]
                              withNumVertices:[self.glView getNumberOfVerticesForCurrentModel]
                       withCurrentRefreshRate:[self.glView currentRefreshRate]];
}

- (BOOL)application:(UIApplication *)application handleOpenURL:(NSURL *)url
{
	if (url != nil)
	  {
    isHandlingCustomURLVTKDownload = YES;
	  }
	// Handle the VTKs custom URL scheme
	[self handleCustomURLScheme:url];
	return YES;
}

-(NSString*) documentsDirectory
{
  NSArray* documentDirectories = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  return [documentDirectories objectAtIndex:0];
}

-(NSString*) pathInDocumentsDirectoryForFileName:(NSString*) fileName
{
  return [[self documentsDirectory] stringByAppendingPathComponent:fileName];
}

-(void) loadBuiltinDatasetWithIndex:(int)index
{
  vesKiwiViewerApp* app = [self.glView getApp];
  NSString* datasetFilename = [NSString stringWithUTF8String:app->builtinDatasetFilename(index).c_str()];  
  NSString* absolutePath = [[NSBundle mainBundle] pathForResource:datasetFilename ofType:nil];
  if (absolutePath == nil)
    {
    absolutePath = [self pathInDocumentsDirectoryForFileName:datasetFilename];
    }

  NSURL* url = [NSURL fileURLWithPath:absolutePath];
  [self handleCustomURLScheme:url];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{ 
  self.lastSessionId = [[alertView textFieldAtIndex:0] text];
  std::string sessionId = [self.lastSessionId UTF8String];
  
  printf("got session id: %s\n", sessionId.c_str());
  
  vesKiwiViewerApp* app = [self.glView getApp];
  
  std::string host = "paraviewweb.kitware.com";
  app->doPVWebTest(host, sessionId);
}

-(void)startPVWebMode
{  
  UIAlertView * alert = [[UIAlertView alloc] initWithTitle:@"Join ParaView Web session"
                                             message:@"Please enter the session id:"
                                             delegate:self
                                             cancelButtonTitle:@"Join"
                                             otherButtonTitles:nil];
  alert.alertViewStyle = UIAlertViewStylePlainTextInput;
  UITextField * alertTextField = [alert textFieldAtIndex:0];
  alertTextField.keyboardType = UIKeyboardTypeNumberPad;
  alertTextField.placeholder = @"session id";
  if (self.lastSessionId != nil)
    {
    alertTextField.placeholder = self.lastSessionId; 
    alertTextField.text = self.lastSessionId;
    }
  [alert show];
  [alert release];
}

-(void)stopPVWebMode
{

}

- (BOOL)handleCustomURLScheme:(NSURL *)url;
{
  //
  // if viewer is not available: bail out
  if (!glView)
    {
    return NO;
    }

  //
  // no url; go with the default dataset
  if (!url)
    {
    NSLog(@"Null url; opening default data file");
    vesKiwiViewerApp* app = [self.glView getApp];
    [self loadBuiltinDatasetWithIndex:app->defaultBuiltinDatasetIndex()];
    return YES;
    }

  NSLog(@"Opening URL: %@", url);

  //
  // we already have a file on the device (e.g., from dropbox); use it
  if ([url isFileURL])
    {
    if ([[url path] hasSuffix:@"pvweb"])
      {
      [self startPVWebMode];
      }
    else
      {
      [self stopPVWebMode];
      [glView setFilePath:[url path]];
      }
    return YES;
    }

  isHandlingCustomURLVTKDownload = YES;

  downloadAlert = [[[UIAlertView alloc] initWithTitle:@"Downloading File\nPlease Wait..." message:nil delegate:self cancelButtonTitle:nil otherButtonTitles: nil] autorelease];
  [downloadAlert show];
  UIActivityIndicatorView *indicator = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
  // Adjust the indicator so it is up a few pixels from the bottom of the alert
  indicator.center = CGPointMake(downloadAlert.bounds.size.width / 2, downloadAlert.bounds.size.height - 50);
  [indicator startAnimating];
  [downloadAlert addSubview:indicator];
  [indicator release];

  NSString *pathComponentForCustomURL = [[url host] stringByAppendingString:[url path]];
  NSString *locationOfRemoteVTKFile = [NSString stringWithFormat:@"http://%@", pathComponentForCustomURL];
  //nameOfDownloadedVTK = @"current.vtk";//[[pathComponentForCustomURL lastPathComponent] retain];
  nameOfDownloadedVTK = [[pathComponentForCustomURL lastPathComponent] retain];

  // Check to make sure that the file has not already been downloaded, if so, just switch to it
  NSString *documentsDirectory = [self documentsDirectory];

  NSLog(@"path = %@",pathComponentForCustomURL);
  NSLog(@"loc = %@",locationOfRemoteVTKFile);
  NSLog(@"name = %@",nameOfDownloadedVTK);
  NSLog(@"docDir = %@",documentsDirectory);

  downloadCancelled = NO;

  // Start download of new file
  [self showDownloadIndicator];

  [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:YES];
  //filename = [documentsDirectory stringByAppendingPathComponent:nameOfDownloadedVTK];

  NSURLRequest *theRequest=[NSURLRequest requestWithURL:[NSURL URLWithString:locationOfRemoteVTKFile]
                                            cachePolicy:NSURLRequestUseProtocolCachePolicy
                                        timeoutInterval:60.0f];
  
  
  downloadConnection = [[NSURLConnection alloc] initWithRequest:theRequest delegate:self];
  if (downloadConnection)
    {
    downloadedFileContents = [[NSMutableData data] retain];
    }
  else
    {
    // inform the user that the download could not be made
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:NSLocalizedStringFromTable(@"Could not open connection", @"Localized", nil) message:[NSString stringWithFormat:NSLocalizedStringFromTable(@"Could not open connection to: %@", @"Localized", nil), nameOfDownloadedVTK]
      delegate:self cancelButtonTitle:NSLocalizedStringFromTable(@"OK", @"Localized", nil) otherButtonTitles: nil, nil];
    [alert show];
    [alert release];		
    return NO;
    }
  
	return YES;
}

#pragma mark -
#pragma mark URL connection delegate methods

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error;
{
	UIAlertView *alert = [[UIAlertView alloc] initWithTitle:NSLocalizedStringFromTable(@"Connection failed", @"Localized", nil) message:NSLocalizedStringFromTable(@"Could not download file", @"Localized", nil)
    delegate:self cancelButtonTitle:NSLocalizedStringFromTable(@"OK", @"Localized", nil) otherButtonTitles: nil, nil];
	[alert show];
	[alert release];
	
	[self downloadCompleted];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
{
	// Concatenate the new data with the existing data to build up the downloaded file
	// Update the status of the download
	
	if (downloadCancelled)
	  {
		[connection cancel];
		[self downloadCompleted];
		downloadCancelled = NO;
		return;
	  }
	[downloadedFileContents appendData:data];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
{
  // This extra check was causing some files from the web not to be loaded.
  /*
	// Stop the spinning wheel and start the status bar for download
	if ([response textEncodingName] != nil)
	  {
		UIAlertView *alert = [[UIAlertView alloc] initWithTitle:NSLocalizedStringFromTable(@"Could not find file", @"Localized", nil) message:[NSString stringWithFormat:NSLocalizedStringFromTable(@"No such file exists on the server: %@", @"Localized", nil), nameOfDownloadedVTK]
                                                   delegate:self cancelButtonTitle:NSLocalizedStringFromTable(@"OK", @"Localized", nil) otherButtonTitles: nil, nil];
		[alert show];
		[alert release];		
		[connection cancel];
		[self downloadCompleted];
		return;
	  }
  */
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection;
{
	// Close off the file and write it to disk
	[self saveFileWithData:downloadedFileContents toFilename:nameOfDownloadedVTK];
	[self downloadCompleted];
}

- (void)saveFileWithData:(NSData *)fileData toFilename:(NSString *)filename;
{
	if (fileData != nil)
    {
    NSString *documentsDirectory = [self documentsDirectory];

    NSError *error = nil;
    BOOL writeStatus;
    writeStatus = [fileData writeToFile:[documentsDirectory stringByAppendingPathComponent:filename] options:NSAtomicWrite error:&error];

    if (!writeStatus)
      {
      UIAlertView *alert = [[UIAlertView alloc] initWithTitle:NSLocalizedStringFromTable(@"Could not write file", @"Localized", nil) message:[NSString stringWithFormat:NSLocalizedStringFromTable(@"Could not write file: %@", @"Localized", nil), nameOfDownloadedVTK]
        delegate:self cancelButtonTitle:NSLocalizedStringFromTable(@"OK", @"Localized", nil) otherButtonTitles: nil, nil];
      [alert show];
      [alert release];		
      return;
      }

    NSString *filePath = [documentsDirectory stringByAppendingPathComponent:filename];
    if(glView)
      {
      [glView setFilePath:filePath];
      }
    }
}

- (void)downloadCompleted;
{
	[downloadConnection release];
	downloadConnection = nil;
	[[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:NO];

	[downloadedFileContents release];
	downloadedFileContents = nil;
	[self hideStatusIndicator];
  [downloadAlert dismissWithClickedButtonIndex:0 animated:YES];
	[nameOfDownloadedVTK release];
	nameOfDownloadedVTK = nil;
}

#pragma mark -
#pragma mark Status update methods

- (void)showStatusIndicator;
{
	[[NSNotificationCenter defaultCenter] postNotificationName:@"FileLoadingStarted" object:NSLocalizedStringFromTable(@"Initializing database...", @"Localized", nil)];
}

- (void)showDownloadIndicator;
{
	[[NSNotificationCenter defaultCenter] postNotificationName:@"FileLoadingStarted" object:NSLocalizedStringFromTable(@"Downloading vtk file...", @"Localized", nil)];
}

- (void)updateStatusIndicator;
{
	
}

- (void)hideStatusIndicator;
{
	[[NSNotificationCenter defaultCenter] postNotificationName:@"FileLoadingEnded" object:nil];
}

-(void)dataSelected:(int)index
{
  if(UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
    {
    [self.viewController dismissModalViewControllerAnimated:NO];
    }
  else
    {
    [self.loadDataPopover dismissPopoverAnimated:YES];
    }

  [self loadBuiltinDatasetWithIndex:index];
}

-(IBAction)setLoadDataButtonTapped:(id)sender
{
  if (_dataLoader == nil)
    {
    _dataLoader = [[LoadDataController alloc]
                    initWithStyle:UITableViewStyleGrouped];
    _dataLoader.delegate = self;

    vesKiwiViewerApp* app = [self.glView getApp];
    NSMutableArray* exampleData = [NSMutableArray array];
    for (int i = 0; i < app->numberOfBuiltinDatasets(); ++i)
    {
      NSString* datasetName = [NSString stringWithUTF8String:app->builtinDatasetName(i).c_str()];
      [exampleData addObject:datasetName];
    }
    _dataLoader.exampleData = exampleData;


    if(UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
      {
      _dataLoader.modalPresentationStyle = UIModalPresentationFormSheet;
      }
    else
      {
      self.loadDataPopover = [[[UIPopoverController alloc]
                               initWithContentViewController:_dataLoader] autorelease];
      }
    }

  if(UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
    {
    TitleBarViewContainer* container = [TitleBarViewContainer new];
    [container addViewToContainer:_dataLoader.view];
    [container setTitle:@"Load Data"];
    container.previousViewController = self.window.rootViewController;    
    [self.viewController presentModalViewController:container animated:YES];    
    }
  else
    {
    [self.loadDataPopover presentPopoverFromRect:[(UIButton*)sender frame]
                                          inView:self.glView permittedArrowDirections:UIPopoverArrowDirectionDown animated:YES];
    self.viewController.loadPopover = self.loadDataPopover;
    }
}

@end
