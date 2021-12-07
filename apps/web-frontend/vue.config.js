module.exports = {
  runtimeCompiler: true,
  publicPath: process.env.CDN_ENV ? process.env.CDN_ENV : '/',

  chainWebpack: config => {
    // workaround taken from webpack/webpack#6642
    config.output
      .globalObject('this')
    // Tell that our main wasm file needs to be loaded by file loader
    config.module
      .rule('mainwasm')
      .test(/stellarium-web-engine\.wasm$/)
      .type('javascript/auto')
      .use('file-loader')
        .loader('file-loader')
        .options({name: '[name].[hash:8].[ext]', outputPath: 'js'})
        .end()
    config.plugin('copy')
      .tap(([pathConfigs]) => {
         const to = pathConfigs[0].to
         pathConfigs[0].force = true // so the original `/public` folder keeps priority
         // add other locations.
         pathConfigs.unshift({
           from: '../data/skydata',
           to: to + '/skydata',
         })
         return [pathConfigs]
       })
  },

  pluginOptions: {
    i18n: {
      locale: 'en',
      fallbackLocale: 'en',
      localeDir: 'locales',
      enableInSFC: true
    }
  }
}
