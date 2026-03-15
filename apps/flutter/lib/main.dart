import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'design_tokens.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
  SystemChrome.setSystemUIOverlayStyle(
    const SystemUiOverlayStyle(
      systemNavigationBarColor: Colors.transparent,
      systemNavigationBarContrastEnforced: false,
      statusBarColor: Colors.transparent,
      statusBarContrastEnforced: false,
    ),
  );
  runApp(const HumanApp());
}

class HumanApp extends StatelessWidget {
  const HumanApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Human',
      debugShowCheckedModeBanner: false,
      theme: _buildTheme(Brightness.light),
      darkTheme: _buildTheme(Brightness.dark),
      themeMode: ThemeMode.system,
      home: const MainScaffold(),
    );
  }

  ThemeData _buildTheme(Brightness brightness) {
    const seedColor = Color(0xFF7AB648);
    final colorScheme = ColorScheme.fromSeed(
      seedColor: seedColor,
      brightness: brightness,
      primary: seedColor,
    );

    final baseTheme = brightness == Brightness.light
        ? ThemeData.light().textTheme
        : ThemeData.dark().textTheme;
    final textTheme = baseTheme.copyWith(
      bodyLarge: baseTheme.bodyLarge?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_body_lg_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_lg_line_height,
      ),
      bodyMedium: baseTheme.bodyMedium?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_body_md_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_md_line_height,
      ),
      bodySmall: baseTheme.bodySmall?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_body_sm_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_sm_line_height,
      ),
      labelLarge: baseTheme.labelLarge?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_label_lg_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_lg_line_height,
      ),
      labelMedium: baseTheme.labelMedium?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_label_md_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_md_line_height,
      ),
      labelSmall: baseTheme.labelSmall?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_label_sm_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_sm_line_height,
      ),
      titleLarge: baseTheme.titleLarge?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_title_lg_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_lg_line_height,
      ),
      titleMedium: baseTheme.titleMedium?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_title_md_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_md_line_height,
      ),
      titleSmall: baseTheme.titleSmall?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_title_sm_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_sm_line_height,
      ),
      headlineMedium: baseTheme.headlineMedium?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_headline_md_font_size.toDouble(),
        fontWeight: FontWeight.w600,
        height: HUTokens.type_role_headline_md_line_height,
      ),
      headlineSmall: baseTheme.headlineSmall?.copyWith(
        fontFamily: 'Avenir',
        fontSize: HUTokens.type_role_headline_sm_font_size.toDouble(),
        fontWeight: FontWeight.w600,
        height: HUTokens.type_role_headline_sm_line_height,
      ),
    );

    final bg = brightness == Brightness.light
        ? HUTokens.light_bg_surface
        : HUTokens.dark_bg_surface;
    final scaffoldBg = brightness == Brightness.light
        ? HUTokens.light_bg
        : HUTokens.dark_bg;
    final textColor = brightness == Brightness.light
        ? HUTokens.light_text
        : HUTokens.dark_text;

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme.copyWith(
        surface: bg,
        onSurface: textColor,
      ),
      scaffoldBackgroundColor: scaffoldBg,
      textTheme: textTheme.apply(
        bodyColor: textColor,
        displayColor: textColor,
      ),
      appBarTheme: AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0,
        systemOverlayStyle: SystemUiOverlayStyle(
          statusBarColor: Colors.transparent,
          statusBarIconBrightness:
              brightness == Brightness.light
                  ? Brightness.dark
                  : Brightness.light,
          systemNavigationBarColor: Colors.transparent,
          systemNavigationBarIconBrightness:
              brightness == Brightness.light
                  ? Brightness.dark
                  : Brightness.light,
        ),
      ),
      bottomNavigationBarTheme: BottomNavigationBarThemeData(
        backgroundColor: brightness == Brightness.light
            ? HUTokens.light_bg_surface
            : HUTokens.dark_bg_surface,
        selectedItemColor: seedColor,
        unselectedItemColor: brightness == Brightness.light
            ? HUTokens.light_text_muted
            : HUTokens.dark_text_muted,
      ),
    );
  }
}

class MainScaffold extends StatefulWidget {
  const MainScaffold({super.key});

  @override
  State<MainScaffold> createState() => _MainScaffoldState();
}

class _MainScaffoldState extends State<MainScaffold> {
  int _currentIndex = 0;

  static const _tabs = [
    ('Overview', Icons.dashboard_outlined, Icons.dashboard),
    ('Chat', Icons.chat_bubble_outline, Icons.chat_bubble),
    ('Sessions', Icons.history_outlined, Icons.history),
    ('Settings', Icons.settings_outlined, Icons.settings),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _buildTabContent(_currentIndex),
      bottomNavigationBar: Container(
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.surface,
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.symmetric(
              horizontal: HUTokens.spacing_md,
              vertical: HUTokens.spacing_sm,
            ),
            child: ClipRRect(
              borderRadius: BorderRadius.circular(HUTokens.radius_lg.toDouble()),
              child: BottomNavigationBar(
                currentIndex: _currentIndex,
                onTap: (i) => setState(() => _currentIndex = i),
                type: BottomNavigationBarType.fixed,
                items: [
                  for (var i = 0; i < _tabs.length; i++)
                    BottomNavigationBarItem(
                      icon: Icon(_currentIndex == i ? _tabs[i].$3 : _tabs[i].$2),
                      label: _tabs[i].$1,
                    ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  /// Lazy screen loading: only build the selected tab (switch, not IndexedStack).
  Widget _buildTabContent(int index) {
    switch (index) {
      case 0:
        return RepaintBoundary(child: const _PlaceholderTab(name: 'Overview'));
      case 1:
        return RepaintBoundary(child: const _PlaceholderTab(name: 'Chat'));
      case 2:
        return RepaintBoundary(child: const _PlaceholderTab(name: 'Sessions'));
      case 3:
        return RepaintBoundary(child: const _PlaceholderTab(name: 'Settings'));
      default:
        return RepaintBoundary(child: const _PlaceholderTab(name: 'Overview'));
    }
  }
}

class _PlaceholderTab extends StatelessWidget {
  const _PlaceholderTab({required this.name});

  final String name;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.transparent,
      body: Center(
        child: Text(
          name,
          style: Theme.of(context).textTheme.headlineMedium,
        ),
      ),
    );
  }
}
