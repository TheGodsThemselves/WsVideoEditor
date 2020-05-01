#import "NativeAddPlugin.h"
#import <native_add/native_add-Swift.h>

@implementation NativeAddPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftNativeAddPlugin registerWithRegistrar:registrar];
}
@end
